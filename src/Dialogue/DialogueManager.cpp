#include "DialogueManager.h"

#include "Util/Random.h"

namespace DDR
{	
	void DialogueManager::Init()
	{
		logger::info("Initializing replacements");
		std::error_code ec{};
		if (!fs::exists(DIRECTORY_PATH, ec) || fs::is_empty(DIRECTORY_PATH, ec)) {
			logger::error("Error loading replacements in {}. Folder is empty or does not exist - {}", DIRECTORY_PATH, ec.message());
			return;
		}
		for (const auto& entry : fs::directory_iterator(DIRECTORY_PATH)) {
			if (entry.is_directory())
				continue;
			const auto ext = entry.path().extension();
			if (ext != ".yml" && ext != ".yaml") {
				continue;
			}
			const std::string fileName = entry.path().string();
			try {
				const auto file = YAML::LoadFile(fileName);
				const auto refs = file["refs"].as<std::unordered_map<std::string, std::string>>(std::unordered_map<std::string, std::string>{});
				const auto refMap = Conditions::ConditionParser::GenerateRefMap(refs);
				size_t responses = ParseResponses(file, refMap);
				size_t topics = ParseTopics(file, refMap);
				size_t scripts = ParseScripts(file);
				logger::info("Loaded {} response replacements, {} topic replacements and {} scripts from {}", responses, topics, scripts, fileName);
			} catch (std::exception& e) {
				logger::info("Failed to load {} - {}", fileName, e.what());
			}
		}
		auto sortByPriority = [](auto& map) {
			for (auto& [_, vec] : map) {
				std::ranges::sort(vec, [](const auto& a, const auto& b) { return a->GetPriority() > b->GetPriority(); });
			}
		};
		sortByPriority(_topicReplacements);
		sortByPriority(_topicReplacementOrphans);
		sortByPriority(_responseReplacements);
	}
	
	size_t DialogueManager::ParseResponses(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs)
	{
		const auto node = a_node["topicInfos"];
		if (!node.IsDefined() || !node.IsSequence()) {
			return 0;
		}
		size_t responses = 0;
		for (const auto&& it : node) {
			try {
				const auto repl = std::make_shared<TopicInfo>(it, a_refs);
				for (const auto& hash : repl->GetHashes()) {
					_responseReplacements[hash].emplace_back(repl);
				}
				responses++;
			} catch (std::exception& e) {
				logger::info("Line {}: Failed to load response replacement - {}", it.Mark().line, e.what());
			}
		}
		return responses;
	}

	size_t DialogueManager::ParseTopics(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs)
	{
		const auto node = a_node["topics"];
		if (!node.IsDefined() || !node.IsSequence()) {
			return 0;
		}
		size_t topics = 0;
		for (const auto&& it : node) {
			try {
				const auto repl = std::make_shared<Topic>(it, a_refs);
				if (auto id = repl->GetId(); id != 0) {
					_topicReplacements[id].emplace_back(repl);
				} else {
					_topicReplacementOrphans[repl->GetAffectedTopic()].emplace_back(repl);
				}
				topics++;
			} catch (std::exception& e) {
				logger::info("Line {}: Failed to load topic replacement - {}", it.Mark().line, e.what());
			}
		}
		return topics;
	}

	size_t DialogueManager::ParseScripts(const YAML::Node& a_node)
	{
		const auto node = a_node["scripts"];
		if (!node.IsDefined() || !node.IsSequence()) {
			return 0;
		}
		size_t scripts = 0;
		for (const auto&& it : node) {
			try {
				TextReplacement repl{ it };
				if (!_lua.InitializeEnvironment(repl)) {
					logger::info("Line {}: Failed to initialize environment for script {}", it.Mark().line, repl.GetScript());
				} else {
					scripts++;
				}
			} catch (std::exception& e) {
				logger::info("Line {}: Failed to load script - {}", it.Mark().line, e.what());
			}
		}
		return scripts;
	}


	RE::TESObjectREFR* DialogueManager::GetDialogueTarget(RE::Actor* a_speaker)
	{
		if (const auto& targetHandle = a_speaker->GetActorRuntimeData().dialogueItemTarget) {
			if (const auto& targetPtr = targetHandle.get()) {
				return targetPtr.get();
			}
		}
		return nullptr;
	}

	std::shared_ptr<TopicInfo> DialogueManager::FindReplacementResponse(RE::Character* a_speaker, RE::TESTopicInfo* a_topicInfo, RE::TESTopicInfo::ResponseData*)
	{
		if (!a_topicInfo || !a_speaker) {
			return nullptr;
		}
		// regular convo between actors
		const auto base = a_speaker->GetActorBase();
		const auto voiceType = base ? base->GetVoiceType() : nullptr;
		if (!voiceType) {
			return nullptr;
		}
		RE::TESObjectREFR* target = GetDialogueTarget(a_speaker);
		// try stored overrides next
		const auto key = TopicInfo::GenerateHash(a_topicInfo->GetFormID(), voiceType);
		auto iter = _responseReplacements.find(key);
		if (iter == _responseReplacements.end()) {
			const auto allKey = TopicInfo::GenerateHash(a_topicInfo->GetFormID());
			iter = _responseReplacements.find(allKey);
		}
		if (iter != _responseReplacements.end()) {
			const auto& replacements = iter->second;
			std::vector<std::shared_ptr<TopicInfo>> candidates;
			for (const auto& repl : replacements) {
				const auto rand = repl->IsRandom();
				if ((candidates.empty() || (rand && repl->GetPriority() >= candidates[0]->GetPriority())) && repl->ConditionsMet(a_speaker, target)) {
					if (rand) {
						candidates.push_back(repl);
					} else {
						return repl;
					}
				}
			}
			if (!candidates.empty()) {
				return candidates[Random::draw<size_t>(0, candidates.size() - 1)];
			}
		}
		return nullptr;
	}

	std::vector<std::shared_ptr<Topic>> DialogueManager::FindReplacementTopic(RE::FormID a_parentId, RE::FormID a_topicId, RE::TESObjectREFR* a_target, bool a_preprocessing)
	{
		std::vector<std::shared_ptr<Topic>> ret{};
		if (_tempTopicMutex.try_lock()) {
			if (_tempTopicKeys.contains(a_parentId)) {
				ret.push_back(_tempTopicReplacements[a_parentId]);
			}
			_tempTopicMutex.unlock();
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto append = [&](const auto& topicMap, const auto findId) {
			auto iter = topicMap.find(findId);
			if (iter == topicMap.end())
				return;
			const auto& replacements = iter->second;
			for (const auto& repl : replacements) {
				if (a_preprocessing && !repl->HasPreProcessingAction())
					continue;
				if (repl->ConditionsMet(player, a_target)) {
					ret.push_back(repl);
				}
			}
		};
		append(_topicReplacements, a_parentId);
		append(_topicReplacementOrphans, a_topicId);
		return ret;
	}

	std::string DialogueManager::AddReplacementTopic(RE::FormID a_topicId, std::string a_text)
	{
		std::unique_lock lock{ _tempTopicMutex };
		std::string key{ Random::generateUUID() };
		if (_tempTopicKeys.count(a_topicId)) {
			logger::info("overwrite detected on {} - previous key = {}", a_topicId, _tempTopicKeys.count(a_topicId));
		}
		_tempTopicKeys[a_topicId] = key;
		_tempTopicReplacements[a_topicId] = std::make_shared<Topic>(a_topicId, a_text);
		return key;
	}

	void DialogueManager::RemoveReplacementTopic(RE::FormID a_topicId, std::string a_key)
	{
		std::unique_lock lock{ _tempTopicMutex };

		if (_tempTopicKeys.count(a_topicId) && _tempTopicKeys[a_topicId] != a_key) {
			return;
		}

		_tempTopicKeys.erase(a_topicId);
		_tempTopicReplacements.erase(a_topicId);
	}
	
	void DialogueManager::ApplyTextReplacements(std::string& a_text, RE::TESObjectREFR* a_speaker, ReplacemenType a_type)
	{
		if (a_text.empty()) {
			return;
		}
		const auto actor = a_speaker ? a_speaker->As<RE::Actor>() : nullptr;
		const auto target = actor ? GetDialogueTarget(actor) : nullptr;
		const uint32_t speakerId = actor ? actor->GetFormID() : 0;
		const uint32_t targetId = target ? target->GetFormID() : 0;
		_lua.ForEachScript([&](const TextReplacement& a_replacement, sol::environment& a_env) {
			if (a_replacement.CanApplyReplacement(a_speaker, target, a_type)) {
				try {
					std::unique_lock lock{ _luaMutex };
					a_env["context"] = std::to_underlying(a_type);
					a_env["speaker_id"] = speakerId;
					a_env["target_id"] = targetId;
					sol::protected_function_result result = a_env["replace"](a_text);
					if (!result.valid()) {
						sol::error err = result;
						logger::error("Failed to apply replacement - {}", err.what());
					} else if (result.get_type() != sol::type::string) {
						const auto type = magic_enum::enum_name(result.get_type());
						logger::error("Failed to apply replacement - expected string, got {}", type);
					} else {
						a_text = result;
					}
				} catch (const std::exception& e) {
					logger::error("Failed to apply replacement - {}", e.what());
				}
			}
		});
	}

	LuaData::LuaData()
	{
		lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::table, sol::lib::math);

		lua.set_function("get_formid", [](uint32_t a_id, const std::string& a_esp) -> uint32_t {
			return RE::TESDataHandler::GetSingleton()->LookupFormID(a_id, a_esp);
		});
		lua.set_function("has_keyword", [](uint32_t a_id, const std::string& a_kwd, bool a_partialMatch) -> int {
			auto form = RE::TESForm::LookupByID<RE::BGSKeywordForm>(a_id);
			if (!form) {
				return -1;
			}
			return a_partialMatch ? form->ContainsKeywordString(a_kwd) : form->HasKeywordString(a_kwd);
		});
		lua.set_function("is_in_faction", [](uint32_t a_id, uint32_t a_faction) -> int {
			auto form = RE::TESForm::LookupByID<RE::Actor>(a_id);
			auto fac = RE::TESForm::LookupByID<RE::TESFaction>(a_faction);
			if (!form || !fac) {
				return -1;
			}
			return form->IsInFaction(fac);
		});
		lua.set_function("has_magic_effect", [](uint32_t a_id, uint32_t a_magicEffect) -> int {
			auto form = RE::TESForm::LookupByID<RE::Actor>(a_id);
			auto mgEff = RE::TESForm::LookupByID<RE::EffectSetting>(a_magicEffect);
			if (!form) {
				return -1;
			}
			return form->AsMagicTarget()->HasMagicEffect(mgEff);
		});
		lua.set_function("get_relationship_rank", [](uint32_t a_id, uint32_t a_target) -> std::string {
			auto form = RE::TESForm::LookupByID<RE::Actor>(a_id);
			auto target = RE::TESForm::LookupByID<RE::Actor>(a_target);
			if (!form || !target) {
				return "";
			}
			auto formBase = form->GetActorBase();
			auto targetBase = target->GetActorBase();
			if (!formBase || !targetBase || !formBase->relationships) {
				return "";
			}
			for (auto&& it : *formBase->relationships) {
				if (it->npc1 == targetBase || it->npc2 == targetBase) {
					auto lv = it->level.get();
					std::string ret{ magic_enum::enum_name(lv) };
					return ret;
				}
			}
			return "";
		});
		lua.set_function("get_sex", [](uint32_t a_id) -> int {
			auto form = RE::TESForm::LookupByID(a_id);
			if (!form) {
				return -1;
			} else if (auto act = form->As<RE::Actor>()) {
				auto base = act->GetActorBase();
				return base ? base->GetSex() : -1;
			} else if (auto npc = form->As<RE::TESNPC>()) {
				return npc->GetSex();
			}
			return -1;
		});
		lua.set_function("get_name", [](uint32_t a_id) -> std::string {
			auto form = RE::TESForm::LookupByID(a_id);
			if (!form) {
				return "NONE";
			}
			std::string ret{ form->GetName() };
			if (ret.empty()) {
				if (auto act = form->As<RE::Actor>()) {
					const auto base = act->GetActorBase();
					return base ? base->GetName() : ret;
				}
			}
			return ret;
		});
		lua.set_function("send_mod_event", [](const std::string& event, const std::string& argStr, float argNum, uint32_t argForm) {
			SKSE::ModCallbackEvent modEvent{
				event,
				argStr,
				argNum,
				argForm ? RE::TESForm::LookupByID(argForm) : nullptr
			};
			SKSE::GetModCallbackEventSource()->SendEvent(&modEvent);
		});
	}

	bool LuaData::InitializeEnvironment(TextReplacement a_replacement)
	{
		sol::environment env{ lua, sol::create, lua.globals() };
		if (!env.valid()) {
			logger::error("Failed to create environment");
			return false;
		}
		const auto scriptName = a_replacement.GetScript();
		const auto scriptPatch = std::format("{}/{}", SCRIPT_PATH, scriptName);
		if (!fs::exists(scriptPatch)) {
			logger::error("Failed to load script. Invalid path - {}", scriptPatch);
			return false;
		}
		lua.script_file(scriptPatch, env);
		if (!env.valid()) {
			logger::error("Failed to load script - {}", scriptPatch);
			return false;
		} else if (!env["replace"].valid()) {
			logger::error("Failed to find replace function");
			return false;
		}
		std::string scriptNameStr{ scriptName };
		env.set_function("log_info", [=](const std::string& message) {
			logger::info("Lua - {} - {}", scriptNameStr, message);
		});
		env.set_function("log_error", [=](const std::string& message) {
			logger::error("Lua - {} - {}", scriptNameStr, message);
		});
		if (!env.valid()) {
			logger::error("Failed to set functions");
			return false;
		}
		scripts.emplace_back(a_replacement, env);
		return true;
	}
	
	void LuaData::ForEachScript(std::function<void(const TextReplacement&, sol::environment&)> a_func)
	{
		for (auto& [repl, env] : scripts) {
			a_func(repl, env);
		}
	}

} // namespace DDR
