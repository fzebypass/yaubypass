#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <string>
#include <vector>

namespace Core::Features::WizardVisuals
{
	inline std::atomic_bool Enabled{ false };
	inline std::atomic<void*> LogoTexture{ nullptr };
	inline std::mutex NamesMutex;
	inline std::vector<std::string> NormalizedNames;

	inline std::string NormalizeName(std::string value)
	{
		value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
			return c < 32 || c == 127;
			}), value.end());

		const auto first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return "";

		const auto last = value.find_last_not_of(" \t\r\n");
		value = value.substr(first, last - first + 1);

		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
			});

		return value;
	}

	inline void SetEnabled(bool enabled)
	{
		Enabled.store(enabled, std::memory_order_release);
	}

	inline bool IsEnabled()
	{
		return Enabled.load(std::memory_order_acquire);
	}

	inline void SetLogoTexture(void* texture)
	{
		LogoTexture.store(texture, std::memory_order_release);
	}

	inline void* GetLogoTexture()
	{
		return LogoTexture.load(std::memory_order_acquire);
	}

	inline void SetNames(std::vector<std::string> names)
	{
		std::vector<std::string> normalized;
		normalized.reserve(names.size());

		for (std::string& name : names)
		{
			std::string clean = NormalizeName(std::move(name));
			if (!clean.empty())
				normalized.push_back(std::move(clean));
		}

		std::sort(normalized.begin(), normalized.end());
		normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());

		std::lock_guard<std::mutex> lock(NamesMutex);
		NormalizedNames = std::move(normalized);
	}

	inline bool MatchesName(const std::string& name)
	{
		const std::string clean = NormalizeName(name);
		if (clean.empty())
			return false;

		std::lock_guard<std::mutex> lock(NamesMutex);
		return std::binary_search(NormalizedNames.begin(), NormalizedNames.end(), clean);
	}
}
