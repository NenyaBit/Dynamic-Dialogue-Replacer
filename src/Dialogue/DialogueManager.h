#pragma once

#include "Topic.h"
#include "TopicInfo.h"

namespace DDR
{
	class DialogueManager
	{
		constexpr static std::string_view DIRECTORY_PATH = "Data\\SKSE\\DynamicDialogueReplacer";

	public:
		static void Init();
		static std::shared_ptr<TopicInfo> FindReplacementResponse(RE::Character* a_speaker, RE::TESTopicInfo* a_topicInfo, RE::TESTopicInfo::ResponseData* a_responseData);
		static std::shared_ptr<Topic> FindReplacementTopic(RE::FormID a_id, RE::TESObjectREFR* a_target, bool a_full);

		static std::string AddReplacementTopic(RE::FormID a_topicId, std::string a_text);
		static void RemoveReplacementTopic(RE::FormID a_topicId, std::string a_key);

	private:
		static inline std::map<std::string, std::vector<std::shared_ptr<TopicInfo>>> _responseReplacements;
		static inline std::unordered_map<RE::FormID, std::vector<std::shared_ptr<Topic>>> _topicReplacements;

		static inline std::unordered_map<RE::FormID, std::string> _tempTopicKeys;
		static inline std::unordered_map<RE::FormID, std::shared_ptr<Topic>> _tempTopicReplacements;
		static inline std::mutex _tempTopicMutex;
	};
}