#include "DialogueManager.h"

#include "Util/Random.h"

namespace DDR
{	
	void DialogueManager::Init()
	{
		logger::info("Initializing replacements");
		std::error_code ec{};
		if (!fs::exists(DIRECTORY_PATH, ec) || fs::is_empty(DIRECTORY_PATH, ec)) {
			logger::error("Error loading replacements in {}: {}", DIRECTORY_PATH, ec.message());
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
				for (const auto&& it : file["topicInfos"]) {
					try {
						const auto repl = std::make_shared<TopicInfo>(it, refMap);
						for (const auto& hash : repl->GetHashes()) {
							_responseReplacements[hash].emplace_back(repl);
						}
					} catch (std::exception& e) {
						logger::info("Failed to load response replaccement - {}", e.what());
					}
				}
				for (const auto&& it : file["topics"]) {
					try {
						const auto repl = std::make_shared<Topic>(it, refMap);
						_topicReplacements[repl->GetId()].emplace_back(repl);
					} catch (std::exception& e) {
						logger::info("Failed to load topic replacement - {}", e.what());
					}
				}
				for (const auto&& it : file["scripts"]) {
					try {
						TextReplacement repl{ it };
						if (!_lua.InitializeEnvironment(repl)) {
							logger::info("Failed to initialize environment for script {}", repl.GetScript());
						}
					} catch (std::exception& e) {
						logger::info("Failed to load script - {}", e.what());
					}
				}
				logger::info("Loaded {} response replacements and {} topic replacements from {}", _responseReplacements.size(), _topicReplacements.size(), fileName);
			} catch (std::exception& e) {
				logger::info("Failed to load {} - {}", fileName, e.what());
			}
		}
		for (auto& [_, replacements] : _topicReplacements) {
			std::ranges::sort(replacements, [](const auto& a, const auto& b) {
				return a->GetPriority() > b->GetPriority();
			});
		}
		for (auto& [_, replacements] : _responseReplacements) {
			std::ranges::sort(replacements, [](const auto& a, const auto& b) {
				return a->GetPriority() > b->GetPriority();
			});
		}
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

	std::shared_ptr<Topic> DialogueManager::FindReplacementTopic(RE::FormID a_id, RE::TESObjectREFR* a_target, bool a_full)
	{
		if (_tempTopicMutex.try_lock()) {
			if (_tempTopicReplacements.count(a_id)) {
				return _tempTopicReplacements[a_id];
			}
			_tempTopicMutex.unlock();
		}
		if (_topicReplacements.count(a_id)) {
			const auto& replacements = _topicReplacements[a_id];
			for (const auto& repl : replacements) {
				if (repl->IsFull() == a_full && repl->ConditionsMet(RE::PlayerCharacter::GetSingleton(), a_target)) {
					return repl;
				}
			}
		}
		return nullptr;
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
		_lua.ForEachScript([&](const TextReplacement& a_replacement, const sol::environment& a_env) {
			if (a_replacement.CanApplyReplacement(a_speaker, a_type)) {
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
		lua.set_function("is_in_faction", [](uint32_t a_id, const uint32_t a_faction) -> int {
			auto form = RE::TESForm::LookupByID<RE::Actor>(a_id);
			auto fac = RE::TESForm::LookupByID<RE::TESFaction>(a_faction);
			if (!form || !fac) {
				return -1;
			}
			return form->IsInFaction(fac);
		});
		lua.set_function("get_sex", [](uint32_t a_id) -> int {
			auto form = RE::TESForm::LookupByID<RE::Actor>(a_id);
			if (!form) {
				return RE::SEX::kNone;
			}
			auto base = form->GetActorBase();
			return base ? base->GetSex() : RE::SEX::kNone;
		});
		lua.set_function("get_name", [](uint32_t a_id) -> std::string {
			auto form = RE::TESForm::LookupByID<RE::Actor>(a_id);
			if (!form) {
				return "";
			}
			return std::string{ form->GetName() };
		});
	}

	bool LuaData::InitializeEnvironment(TextReplacement a_replacement)
	{
		sol::environment env{ lua, sol::create };
		if (!env.valid()) {
			logger::error("Failed to create environment");
			return false;
		}
		const auto scriptPatch = std::format("{}/{}", SCRIPT_PATH, a_replacement.GetScript());
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
		env.set_function("log_info", [script = a_replacement.GetScript()](const std::string& message) {
			logger::info("Lua - {} - {}", script, message);
		});
		env.set_function("log_error", [script = a_replacement.GetScript()](const std::string& message) {
			logger::error("Lua - {} - {}", script, message);
		});
		if (!env.valid()) {
			logger::error("Failed to set functions");
			return false;
		}
		scripts.emplace_back(a_replacement, env);
		return true;
	}
	
	void LuaData::ForEachScript(std::function<void(const TextReplacement&, const sol::environment&)> a_func) const
	{
		for (const auto& [repl, env] : scripts) {
			a_func(repl, env);
		}
	}

} // namespace DDR
