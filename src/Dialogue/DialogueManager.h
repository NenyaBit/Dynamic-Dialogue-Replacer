#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <lua.hpp>
#include <sol/sol.hpp>

#include "Util/Singleton.h"
#include "TextReplacement.h"
#include "Topic.h"
#include "TopicInfo.h"

namespace DDR
{
	constexpr static std::string_view DIRECTORY_PATH = "Data\\SKSE\\DynamicDialogueReplacer";
	constexpr static std::string_view SCRIPT_PATH = "Data\\SKSE\\DynamicDialogueReplacer\\Scripts";

	struct LuaData
	{
		LuaData();
		~LuaData() { lua.collect_garbage(); }

		bool InitializeEnvironment(TextReplacement a_replacement);
		void ForEachScript(std::function<void(const TextReplacement&, sol::environment&)> a_func);

	private:
		sol::state lua{};
		std::vector<std::pair<TextReplacement, sol::environment>> scripts{};
	};

	class DialogueManager : 
		public Singleton<DialogueManager>
	{
	public:
		static RE::TESObjectREFR* GetDialogueTarget(RE::Actor* a_speaker);

	public:
		void Init();
		std::shared_ptr<TopicInfo> FindReplacementResponse(RE::Character* a_speaker, RE::TESTopicInfo* a_topicInfo, RE::TESTopicInfo::ResponseData* a_responseData);
		std::vector<std::shared_ptr<Topic>> FindReplacementTopic(RE::FormID a_parentId, RE::FormID a_topicId, RE::TESObjectREFR* a_target, bool a_preprocessing);

		std::string AddReplacementTopic(RE::FormID a_topicId, std::string a_text);
		void RemoveReplacementTopic(RE::FormID a_topicId, std::string a_key);
		void ApplyTextReplacements(std::string& a_text, RE::TESObjectREFR* a_speaker, ReplacemenType a_type);

	private:
		size_t ParseResponses(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs);
		size_t ParseTopics(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs);
		size_t ParseScripts(const YAML::Node& a_node);

	private:
		LuaData _lua{};
		std::mutex _luaMutex{};
		std::map<std::string, std::vector<std::shared_ptr<TopicInfo>>> _responseReplacements;
		std::unordered_map<RE::FormID, std::vector<std::shared_ptr<Topic>>> _topicReplacements;
		std::unordered_map<RE::FormID, std::vector<std::shared_ptr<Topic>>> _topicReplacementOrphans;	// Replacements without a parent topic

		std::unordered_map<RE::FormID, std::string> _tempTopicKeys;
		std::unordered_map<RE::FormID, std::shared_ptr<Topic>> _tempTopicReplacements;
		std::mutex _tempTopicMutex{};
	};
}	 // namespace DDR
