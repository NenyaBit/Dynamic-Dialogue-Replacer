#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <lua.hpp>
#include <sol/sol.hpp>

#include "TextReplacement.h"
#include "Topic.h"
#include "TopicInfo.h"

namespace DDR
{
	constexpr static std::string_view DIRECTORY_PATH = "Data\\SKSE\\DynamicDialogueReplacer";
	constexpr static std::string_view SCRIPT_PATH = "Data\\SKSE\\Plugins\\DynamicDialogueReplacer\\Scripts";

	struct LuaData
	{
		LuaData();
		~LuaData() { lua.collect_garbage(); }

		bool InitializeEnvironment(TextReplacement a_replacement);
		void ForEachScript(std::function<void(const TextReplacement&, const sol::environment&)> a_func) const;

	private:
		sol::state lua{};
		std::vector<std::pair<TextReplacement, sol::environment>> scripts{};
	};

	class DialogueManager
	{
	public:
		static RE::TESObjectREFR* GetDialogueTarget(RE::Actor* a_speaker);

	public:
		static void Init();
		static std::shared_ptr<TopicInfo> FindReplacementResponse(RE::Character* a_speaker, RE::TESTopicInfo* a_topicInfo, RE::TESTopicInfo::ResponseData* a_responseData);
		static std::shared_ptr<Topic> FindReplacementTopic(RE::FormID a_id, RE::TESObjectREFR* a_target, bool a_full);

		static std::string AddReplacementTopic(RE::FormID a_topicId, std::string a_text);
		static void RemoveReplacementTopic(RE::FormID a_topicId, std::string a_key);
		static void ApplyTextReplacements(std::string& a_text, RE::TESObjectREFR* a_speaker, ReplacemenType a_type);

	private:
		static inline LuaData _lua{};
		static inline std::map<std::string, std::vector<std::shared_ptr<TopicInfo>>> _responseReplacements;
		static inline std::unordered_map<RE::FormID, std::vector<std::shared_ptr<Topic>>> _topicReplacements;

		static inline std::unordered_map<RE::FormID, std::string> _tempTopicKeys;
		static inline std::unordered_map<RE::FormID, std::shared_ptr<Topic>> _tempTopicReplacements;
		static inline std::mutex _tempTopicMutex;
	};
}	 // namespace DDR
