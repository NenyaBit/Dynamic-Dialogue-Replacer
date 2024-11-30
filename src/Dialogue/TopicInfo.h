#pragma once

#include "Conditions/Conditional.h"
#include "Util/FormLookup.h"
#include "Util/StringUtil.h"

namespace DDR
{
	struct Response
	{
		bool keep;
		std::string sub;
		std::string path;
	};

	class TopicInfo
	{
	public:
		TopicInfo(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs);
		~TopicInfo() = default;

		static inline std::string GenerateHash(RE::FormID a_topicInfoId, RE::BGSVoiceType* a_voiceType)
		{
			if (!a_voiceType)
				return "";

			return std::format("{}|{}", a_topicInfoId, a_voiceType->GetFormEditorID());
		}
		static inline std::string GenerateHash(RE::FormID a_topicInfoId)
		{
			return std::format("{}|all", a_topicInfoId);
		}
		inline std::vector<std::string> GetHashes() const
		{
			return _voiceTypes.empty() ? std::vector<std::string>{ GenerateHash(_topicInfoId) } :
																	 _voiceTypes | std::ranges::views::transform([this](RE::BGSVoiceType* a_voiceType) {
																		 return GenerateHash(_topicInfoId, a_voiceType);
																	 }) | std::ranges::to<std::vector>();
		}

		inline bool HasReplacement(int a_num) { return a_num <= _responses.size() && !_responses[a_num - 1].keep; }
		inline bool HasReplacementSub(int a_num) { return HasReplacement(a_num) && !_responses[a_num - 1].sub.empty(); }
		inline bool HasReplacementPath(int a_num) { return HasReplacement(a_num) && !_responses[a_num - 1].path.empty(); }

		inline std::string GetSubtitle(int a_num) { return _responses[a_num - 1].sub; }
		inline std::string GetPath(RE::TESTopic* a_topic, RE::TESTopicInfo* a_topicInfo, RE::BGSVoiceType* a_voiceType, int a_num)
		{
			const auto path{ _responses[a_num - 1].path };

			if (path[0] != '$')
				return path;

			auto sections = Util::StringSplitToOwned(path, "\\"sv);
			for (auto& section : sections) {
				if (section == "[VOICE_TYPE]") {
					section = a_voiceType->GetFormEditorID();
				} else if (section == "[TOPIC_MOD_FILE]") {
					section = a_topic->GetFile()->GetFilename();
				} else if (section == "[TOPIC_INFO_MOD_FILE]") {
					section = a_topicInfo->GetFile()->GetFilename();
				} else if (section == "[VOICE_MOD_FILE]") {
					section = a_voiceType->GetDescriptionOwnerFile()->GetFilename();
				}
			}

			return Util::StringJoin(sections, "\\"sv);
		}
		inline bool IsRand() { return _random; }
		inline uint64_t GetPriority() { return _priority; }
		inline bool ShouldCut(int a_num) { return _cut && a_num >= _responses.size(); }
		inline bool ConditionsMet(RE::TESObjectREFR* a_subject, RE::TESObjectREFR* a_target) { return _conditions.ConditionsMet(a_subject, a_target); }

	private:
		RE::FormID _topicInfoId;
		std::vector<Response> _responses;
		std::vector<RE::BGSVoiceType*> _voiceTypes{};
		Conditions::Conditional _conditions{};
		uint64_t _priority{ 0 };
		bool _random{ false };
		bool _cut{ true };
	};
}

namespace YAML
{
	template <>
	struct convert<DDR::Response>
	{
		static bool decode(const Node& node, DDR::Response& rhs)
		{
			rhs.sub = node["sub"].as<std::string>("");
			rhs.path = node["path"].as<std::string>("");
			rhs.keep = node["keep"].as<std::string>("") == "true" || node["keep"].as<bool>(false);
			return true;
		}
	};
}
