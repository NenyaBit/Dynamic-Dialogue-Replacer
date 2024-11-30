#include "Topic.h"

namespace DDR
{
	Topic::Topic(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs) :
		_id(Util::FormFromString(a_node["id"].as<std::string>())),
		_text(a_node["text"].as<std::string>("")),
		_priority(a_node["priority"].as<uint64_t>(0)),
		_conditions(Conditions::Conditional{ a_node["conditions"].as<std::vector<std::string>>(std::vector<std::string>{}), a_refs }),
		_with(Util::FormFromString<RE::TESTopic>(a_node["with"].as<std::string>(""))),
		_inject(a_node["inject"].as<std::vector<std::string>>(std::vector<std::string>{}) |
						std::ranges::views::transform([](const auto& str) { return Util::FormFromString<RE::TESTopic>(str); }) |
						std::ranges::views::filter([](const auto it) { return it != nullptr; }) |
						std::ranges::to<std::vector>()),
		_proceed(a_node["proceed"].as<std::string>("true") == "true" || a_node["proceed"].as<bool>(true)),
		_check(a_node["check"].as<std::string>("") == "true" || a_node["check"].as<bool>(false)),
		_hide(a_node["hide"].as<std::string>("") == "true" || a_node["hide"].as<bool>(false))
	{
		if (_id == 0 || !RE::TESForm::LookupByID(_id)) {
			throw std::runtime_error("Failed to obtain topic");
		}
		// COMEBACK: _inject is marked as "not empty" (!_inject.empty()) as a condition in the original implementation
		if (_text.empty() && !_with && _inject.empty()) {
			throw std::runtime_error("Failed to find text or with/inject topics in replacement");
		}
	}

	Topic::Topic(RE::FormID a_id, std::string a_text) :
		_id(a_id), _text(a_text)
	{
		if (_id == 0 || !RE::TESForm::LookupByID(_id)) {
			throw std::runtime_error("Failed to obtain topic");
		}
	}
}	 // namespace DDR
