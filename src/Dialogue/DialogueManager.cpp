#include "DialogueManager.h"

#include "Util/Random.h"

namespace DDR
{
	void DialogueManager::Init()
	{
		// TODO: parse all files in DynamicDialogueReplacer folder

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
							_respReplacements[hash].emplace_back(_responses.back());
						}
						_responses.push_back(repl);
					} catch (std::exception& e) {
						logger::info("Failed to load response replaccement - {}", e.what());
					}
				}
				for (const auto&& it : file["topics"]) {
					try {
						const auto repl = std::make_shared<Topic>(it, refMap);
						_topicReplacements[repl->GetId()].emplace_back(_topics.back());
						_topics.push_back(repl);
					} catch (std::exception& e) {
						logger::info("Failed to load topic replacement - {}", e.what());
					}
				}
				logger::info("Loaded {} response replacements and {} topic replacements from {}", _respReplacements.size(), _topicReplacements.size(), fileName);
			} catch (std::exception& e) {
				logger::info("Failed to load {} - {}", fileName, e.what());
			}
		}

		for (auto& [_, replacements] : _topicReplacements) {
			std::ranges::sort(replacements, [](const auto& a, const auto& b) {
				return a->GetPriority() > b->GetPriority();
			});
		}

		for (auto& [_, replacements] : _respReplacements) {
			std::ranges::sort(replacements, [](const auto& a, const auto& b) {
				return a->GetPriority() > b->GetPriority();
			});
		}
	}

	std::shared_ptr<TopicInfo> DialogueManager::FindReplacementResponse(RE::Character* a_speaker, RE::TESTopicInfo* a_topicInfo, RE::TESTopicInfo::ResponseData* a_responseData)
	{
		if (!a_topicInfo || !a_responseData) {
			return nullptr;
		}

		RE::Actor* speaker = a_speaker;
		RE::TESObjectREFR* target = nullptr;
		RE::BGSVoiceType* voiceType = nullptr;
		if (speaker) {
			// regular convo between actors
			if (const auto base = speaker->GetActorBase()) {
				voiceType = base->GetVoiceType();
			}

			if (const auto& targetHandle = a_speaker->GetActorRuntimeData().dialogueItemTarget) {
				if (const auto& targetPtr = targetHandle.get()) {
					target = targetPtr.get();
				}
			}
		}

		if (!speaker || !voiceType) {
			return nullptr;
		}

		const auto key = TopicInfo::GenerateHash(a_topicInfo->GetFormID(), voiceType);
		const auto allKey = TopicInfo::GenerateHash(a_topicInfo->GetFormID());

		// try stored overrides next
		auto iter = _respReplacements.find(key);
		if (iter == _respReplacements.end()) {
			iter = _respReplacements.find(allKey);
		}

		if (iter != _respReplacements.end()) {
			const auto& replacements = iter->second;

			std::vector<std::shared_ptr<TopicInfo>> candidates;

			for (const auto& repl : replacements) {
				const auto rand = repl->IsRand();
				if ((candidates.empty() || (rand && repl->GetPriority() >= candidates[0]->GetPriority())) && repl->ConditionsMet(speaker, target)) {
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
} // namespace DDR
