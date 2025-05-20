#pragma once

#include "Dialogue/DialogueManager.h"
#include <unordered_set>

namespace RE
{
	int64_t AddTopic(RE::MenuTopicManager* a_this, RE::TESTopic* a_topic, int64_t a_3, int64_t a_4);
}

namespace DDR
{
	typedef int64_t(WINAPI* PopulateTopicInfoType)(int64_t a_1, RE::TESTopic* a_2, RE::TESTopicInfo* a_3, RE::Character* a_4, RE::TESTopicInfo::ResponseData* a_5);

	class Hooks
	{
	public:
		static void Install();

	private:
		struct Response
		{
			RE::Character* speaker{ nullptr };
			std::shared_ptr<TopicInfo> response{ nullptr };
			int32_t responseNumber{ -1 };
		};

		static int64_t PopulateTopicInfo(int64_t a_1, RE::TESTopic* a_2, RE::TESTopicInfo* a_3, RE::Character* a_4, RE::TESTopicInfo::ResponseData* a_5);
		static inline PopulateTopicInfoType _PopulateTopicInfo;

		static char* SetSubtitle(RE::DialogueResponse* a_response, char* text, int32_t unk);
		static inline REL::Relocation<decltype(SetSubtitle)> _SetSubtitle;

		static bool ConstructResponse(RE::TESTopicInfo::ResponseData* a_response, char* a_filePath, RE::BGSVoiceType* a_voiceType, RE::TESTopic* a_topic, RE::TESTopicInfo* a_topicInfo);
		static inline REL::Relocation<decltype(ConstructResponse)> _ConstructResponse;

		thread_local static inline Response _response{};

	private:
		static inline int64_t AddTopic(RE::MenuTopicManager* a_this, RE::TESTopic* a_topic, RE::TESTopic* a_activeTopic, uint64_t a_4);
		static inline REL::Relocation<decltype(AddTopic)> _AddTopic;
	};

	class DialogueMenuEx : public RE::DialogueMenu
	{
	public:
		static inline void Install()
		{
			REL::Relocation<uintptr_t> vtbl(RE::VTABLE_DialogueMenu[0]);
			_ProcessMessageFn = vtbl.write_vfunc(0x4, &ProcessMessageEx);
		}

		RE::UI_MESSAGE_RESULTS ProcessMessageEx(RE::UIMessage& a_message);

	private:
		RE::FormID _activeRootId{ 0 };

		using ProcessMessageFn = decltype(&RE::DialogueMenu::ProcessMessage);
		static inline REL::Relocation<ProcessMessageFn> _ProcessMessageFn;
	};
}
