#pragma once

#include "Conditions/Conditional.h"
#include "Util/FormLookup.h"

namespace DDR
{
	class Topic
	{
	public:
		Topic(const YAML::Node& a_node, const Conditions::ConditionParser::RefMap& a_refs);
		Topic(RE::FormID a_id, std::string a_text);
		~Topic() = default;

		_NODISCARD RE::FormID GetId() { return _id; }
		_NODISCARD const char* GetText() { return _text.c_str(); }
		_NODISCARD bool IsHidden() { return _hide; }
		_NODISCARD bool IsFull() { return _with != nullptr || _hide || !_inject.empty(); }
		_NODISCARD bool ShouldProceed() { return _proceed; }
		_NODISCARD RE::TESTopic* GetTopic() { return _with; }
		_NODISCARD const std::vector<RE::TESTopic*>& GetInjections() { return _inject; }
		_NODISCARD uint64_t GetPriority() { return _priority; }
		_NODISCARD bool GetCheck() { return _check; }
		_NODISCARD bool ConditionsMet(RE::TESObjectREFR* a_subject, RE::TESObjectREFR* a_target) { return _conditions.ConditionsMet(a_subject, a_target); }

	private:
		RE::FormID _id;
		std::string _text;
		RE::TESTopic* _with{ nullptr };
		std::vector<RE::TESTopic*> _inject{};
		Conditions::Conditional _conditions{};
		uint64_t _priority{ 0 };
		bool _proceed{ true };
		bool _check{ false };
		bool _hide{ false };
	};
} // namespace DDR
