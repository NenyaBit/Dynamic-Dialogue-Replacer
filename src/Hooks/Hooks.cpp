#include "Hooks.h"

namespace RE
{
	int64_t AddTopic(RE::MenuTopicManager* a_this, RE::TESTopic* a_topic, int64_t a_3, int64_t a_4)
	{
		// add SE address
		using func_t = decltype(&RE::AddTopic);
		REL::Relocation<func_t> func{ REL::ID(35303) };
		return func(a_this, a_topic, a_3, a_4);
	}
}	 // namespace RE

namespace DDR
{
	void Hooks::Install()
	{
		auto& trampoline = SKSE::GetTrampoline();
		SKSE::AllocTrampoline(64);

		REL::Relocation<std::uintptr_t> target{ REL::RelocationID(34429, 35249) };
		_SetSubtitle = trampoline.write_call<5>(target.address() + REL::Relocate(0x61, 0x61), SetSubtitle);
		_ConstructResponse = trampoline.write_call<5>(target.address() + REL::Relocate(0xDE, 0xDE), ConstructResponse);

		const uintptr_t addr = target.address();
		_PopulateTopicInfo = (PopulateTopicInfoType)addr;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)_PopulateTopicInfo, (PBYTE)&PopulateTopicInfo);
		if (DetourTransactionCommit() != NO_ERROR) {
			logger::error("Failed to install hook on PopulateTopicInfo");
		}

		_AddTopic = trampoline.write_call<5>(REL::Relocation<std::uintptr_t>{ REL::VariantID{ 34460, 35287, 0x05766F0 } }.address() + REL::Relocate(0xFA, 0x154), AddTopic);
		trampoline.write_call<5>(REL::Relocation<std::uintptr_t>{ REL::VariantID{ 34477, 35304, 0x05778D0 } }.address() + REL::Relocate(0x79, 0x6C), AddTopic);

		DialogueMenuEx::Install();

		logger::info("installed hooks");
	}

	int64_t Hooks::PopulateTopicInfo(int64_t a_1, RE::TESTopic* a_2, RE::TESTopicInfo* a_3, RE::Character* a_speaker, RE::TESTopicInfo::ResponseData* a_5)
	{
		_response.responseNumber = a_5->responseNumber;
		if (_response.responseNumber == 1) {
			_response.response = DialogueManager::GetSingleton()->FindReplacementResponse(a_speaker, a_3, a_5);
			_response.speaker = a_speaker;
		}
		if (_response.response && _response.response->ShouldCut(_response.responseNumber)) {
			delete a_5->next;
			a_5->next = nullptr;
		}
		return _PopulateTopicInfo(a_1, a_2, a_3, a_speaker, a_5);
	}

	char* Hooks::SetSubtitle(RE::DialogueResponse* a_response, char* a_text, int32_t a_3)
	{
		std::string text;
		if (_response.response && _response.response->HasReplacementSubtitle(_response.responseNumber)) {
			const auto replace = _response.response->GetSubtitle(_response.responseNumber);
			logger::info("replacing subtitle {} with {}", a_text, replace);
			text = replace;
		} else {
			text = a_text;
		}
		DialogueManager::GetSingleton()->ApplyTextReplacements(text, _response.speaker, ReplacemenType::Response);
		return _SetSubtitle(a_response, text.data(), a_3);
	}

	bool Hooks::ConstructResponse(RE::TESTopicInfo::ResponseData* a_response, char* a_filePath, RE::BGSVoiceType* a_voiceType, RE::TESTopic* a_topic, RE::TESTopicInfo* a_topicInfo)
	{
		if (!_ConstructResponse(a_response, a_filePath, a_voiceType, a_topic, a_topicInfo)) {
			return false;
		}
		if (_response.response && _response.response->HasReplacementVoiceFile(_response.responseNumber)) {
			std::string filePath{ a_filePath };
			const auto path = _response.response->GetVoiceFilePath(a_topic, a_topicInfo, a_voiceType, a_response->responseNumber);
			logger::info("replacing voice file {} with {}", filePath, path);
			*a_filePath = NULL;
			strcat_s(a_filePath, 0x104ui64, path.c_str());
		}
		return true;
	}

	int64_t Hooks::AddTopic(RE::MenuTopicManager* a_this, RE::TESTopic* a_topic, RE::TESTopic* a_activeTopic, uint64_t a_4)
	{
		if (!a_activeTopic) {
			return _AddTopic(a_this, a_topic, a_activeTopic, a_4);
		}
		const auto target = a_this->speaker.get().get();
		auto topics = DialogueManager::GetSingleton()->FindReplacementTopic(a_activeTopic->GetFormID(), target, true);
		if (topics.empty()) {
			return _AddTopic(a_this, a_topic, a_activeTopic, a_4);
		}
		bool hasValidResponse = false;
		auto currInfo = a_activeTopic->topicInfos;
		for (auto i = a_activeTopic->numTopicInfos; i > 0; i--) {
			if (currInfo && *currInfo) {
				if ((*currInfo)->objConditions.IsTrue(target, RE::PlayerCharacter::GetSingleton())) {
					hasValidResponse = true;
					break;
				}
			}
			currInfo++;
		}
		bool firstPass = !a_this->dialogueList || a_this->dialogueList->empty();
		for (auto&& it : topics) {
			if (!hasValidResponse && it->VerifyExistingConditions()) {
				continue;
			}
			if (firstPass) {
				// Inject additional topics
				for (const auto& injectTopic : it->GetInjections()) {
					_AddTopic(a_this, injectTopic, a_activeTopic, a_4);
				}
			}
			if (it->AffectsInfoTopic(a_topic)) {
				// Hide topic, consumes the topic
				if (it->IsHidden()) {
					return 0;
				}
				// Replace active sub-topic
				if (const auto replace = it->GetReplacingTopic()) {
					a_topic = replace;
				}
			}
			if (!it->ShouldProceed()) {
				break;
			}
		}
		return _AddTopic(a_this, a_topic, a_activeTopic, a_4);
	}

	RE::UI_MESSAGE_RESULTS DialogueMenuEx::ProcessMessageEx(RE::UIMessage& a_message)
	{
		static std::unordered_map<RE::FormID, std::string> cache;
		const auto menu = RE::MenuTopicManager::GetSingleton();
		const auto manager = DialogueManager::GetSingleton();
		switch (*a_message.type) {
		case RE::UI_MESSAGE_TYPE::kShow:
		case RE::UI_MESSAGE_TYPE::kUpdate:
			if (const auto dialogue = menu->dialogueList) {
#pragma warning(suppress : 4834)
				for (auto it = dialogue->begin(); it != dialogue->end(); it++) {
					const auto activeInfo = *it;
					if (!activeInfo) {
						continue;
					}
					const auto formId = activeInfo->parentTopic->GetFormID();
					auto where = cache.find(formId);
					if (where != cache.end()) {
						activeInfo->topicText = where->second;
						continue;
					}
					const auto speaker = menu->speaker.get().get();
					auto topics = manager->FindReplacementTopic(formId, speaker, false);
					std::string text{ activeInfo->topicText.c_str() };
					for (auto&& topic : topics) {
						if (!topic->GetText().empty()) {
							text = topic->GetText();
							break;
						}
					}
					manager->ApplyTextReplacements(text, speaker, ReplacemenType::Topic);
					activeInfo->topicText = text;
					cache[formId] = text;
				}
			}
			break;
		case RE::UI_MESSAGE_TYPE::kHide:
			cache.clear();
			break;
		}
		return _ProcessMessageFn(this, a_message);
	}

}	 // namespace DDR
