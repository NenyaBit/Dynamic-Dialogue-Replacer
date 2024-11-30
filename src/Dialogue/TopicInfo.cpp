#include "TopicInfo.h"

namespace DDR
{
	TopicInfo::TopicInfo(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs) :
		_topicInfoId(Util::FormFromString(a_node["id"].as<std::string>())),
		_responses(a_node["responses"].as<std::vector<Response>>(std::vector<Response>{})),
		_priority(a_node["priority"].as<uint64_t>(0)),
		_conditions(Conditions::Conditional{ a_node["conditions"].as<std::vector<std::string>>(std::vector<std::string>{}), a_refs }),
		_voiceTypes(a_node["voices"].as<std::vector<std::string>>(std::vector<std::string>{}) |
								std::ranges::views::transform([](const auto& str) { return RE::TESForm::LookupByEditorID<RE::BGSVoiceType>(str); }) |
								std::ranges::views::filter([](const auto it) { return it != nullptr; }) |
								std::ranges::to<std::vector>()),
		_random(a_node["random"].as<std::string>("") == "true" || a_node["random"].as<bool>(false)),
		_cut(a_node["cut"].as<std::string>("true") == "true" || a_node["cut"].as<bool>(true))
	{
		if (_topicInfoId == 0) {
			throw std::runtime_error("Failed to obtain topic info");
		}
		if (_responses.empty()) {
			const auto err = std::format("Failed to find responses in replacement {}", _topicInfoId);
			throw std::runtime_error(err.c_str());
		}
	}

} // namespace DDR
