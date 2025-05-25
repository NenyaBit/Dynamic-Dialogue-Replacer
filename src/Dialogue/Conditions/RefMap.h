#pragma once

#include "Util/FormLookup.h"

namespace Conditions
{
	class RefMap
	{
		struct StringCmp
		{
			bool operator()(const std::string& a_lhs, const std::string& a_rhs) const
			{
				return _strcmpi(a_lhs.c_str(), a_rhs.c_str()) < 0;
			}
		};

		std::map<std::string, RE::TESForm*, StringCmp> refMap;

	public:
		template <typename T = RE::TESForm>
		T* Lookup(const std::string& a_key) const
		{
			if (a_key.empty()) {
				return nullptr;
			}
			if (refMap.contains(a_key)) {
				const auto ret = refMap.at(a_key);
				return ret->As<T>();
			}
			auto ref = Util::FormFromString<T>(a_key);
			if (!ref) {
				ref = RE::TESForm::LookupByEditorID<T>(a_key);
				if (!ref) {
					logger::error("Failed to lookup form for key: {}", a_key);
					return nullptr;
				}
			}
			return ref;
		}

		RE::FormID LookupId(const std::string& a_key) const
		{
			const auto ref = Lookup(a_key);
			if (ref) {
				return ref->GetFormID();
			}
			logger::warn("Failed to validate form for key: {} (This may be expected for Topic Infos)", a_key);
			return Util::FormFromString(a_key);
		}

	public:
		RefMap(const std::vector<std::pair<std::string, std::string>>& a_rawRefs)
		{
			refMap["player"] = RE::PlayerCharacter::GetSingleton();
			for (const auto& [key, refStr] : a_rawRefs) {
				if (auto ref = Lookup<RE::TESForm>(refStr)) {
					refMap[key] = ref;
				}
			}
		}
		~RefMap() = default;
	};

}	 // namespace Conditions