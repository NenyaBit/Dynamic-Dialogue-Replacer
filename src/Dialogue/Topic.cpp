#include "Topic.h"

namespace DDR
{
	Topic::Topic(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs) :
		_affectedTopic(Util::FormFromString(a_node["id"].as<std::string>())),
		_affectedInfo(Util::FormFromString(a_node["affects"].as<std::string>(""))),
		_replaceWith(Util::FormFromString(a_node["replace"].as<std::string>(""))),
		_text(a_node["text"].as<std::string>("")),
		_inject(a_node["inject"].as<std::vector<std::string>>(std::vector<std::string>{}) |
						std::ranges::views::transform([](const auto& str) { return Util::FormFromString<RE::TESTopic>(str); }) |
						std::ranges::views::filter([](const auto it) { return it != nullptr; }) |
						std::ranges::to<std::vector>()),
		_conditions(Conditions::Conditional{ a_node["conditions"].as<std::vector<std::string>>(std::vector<std::string>{}), a_refs }),
		_priority(a_node["priority"].as<uint64_t>(0)),
		_proceed(a_node["proceed"].as<std::string>("true") == "true" || a_node["proceed"].as<bool>(true)),
		_check(a_node["check"].as<std::string>("") == "true" || a_node["check"].as<bool>(false)),
		_hide(a_node["hide"].as<std::string>("") == "true" || a_node["hide"].as<bool>(false))
	{
		if (!_affectedTopic || !RE::TESForm::LookupByID<RE::TESTopic>(_affectedTopic)) {
			const YAML::Node& errored_node = a_node["id"];
			const auto err = std::format("Line {}:{}: Failed to obtain id topic '{}'", errored_node.Mark().line, errored_node.Mark().column, errored_node.as<std::string>());
			throw std::runtime_error(err);
		}
		if (_affectedInfo && !RE::TESForm::LookupByID<RE::TESTopic>(_affectedInfo)) {
			const YAML::Node& errored_node = a_node["affects"];
			const auto err = std::format("Line {}:{}: Failed to obtain affected topic '{}'. Did you state a TopicInfo ID instead of a Topic ID?", errored_node.Mark().line, errored_node.Mark().column, errored_node.as<std::string>());
			throw std::runtime_error(err);
		}
		if (_replaceWith) {
			if (!RE::TESForm::LookupByID<RE::TESTopic>(_replaceWith)) {
				const YAML::Node& errored_node = a_node["replace"];
				const auto err = std::format("Line {}:{}: Failed to obtain replacement topic '{}'", errored_node.Mark().line, errored_node.Mark().column, errored_node.as<std::string>());
				throw std::runtime_error(err);
			} else if (!_affectedInfo) {
				const YAML::Node& errored_node = a_node;
				const auto err = std::format("Line {}:{}: Missing affected topic. Replacement must specify affected info topic when replacing topic", errored_node.Mark().line, errored_node.Mark().column);
				throw std::runtime_error(err);
			} 
		}
		if (_text.empty() && !_replaceWith && _inject.empty() && !_hide) {
			const YAML::Node& errored_node = a_node;
			const auto err = std::format("Line {}:{}: Failed to find text or with/inject topics in replacement", errored_node.Mark().line, errored_node.Mark().column);
			throw std::runtime_error(err);
		}
	}

	Topic::Topic(RE::FormID a_id, std::string a_text) :
		_affectedTopic(a_id), _text(a_text)
	{
		if (_affectedTopic == 0 || !RE::TESForm::LookupByID<RE::TESTopic>(_affectedTopic)) {
			throw std::runtime_error("Failed to obtain topic");
		}
	}
}	 // namespace DDR
