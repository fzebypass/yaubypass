#pragma once

#include <Includes/Includes.hpp>
#include <Core/SDK/SDK.hpp>
#include <array>
#include <atomic>

namespace Core::Features::TargetHighlight
{
	enum class Source : size_t
	{
		Aimbot = 0,
		SilentAim,
		Triggerbot,
		Count
	};

	inline std::array<std::atomic<CPed*>, static_cast<size_t>(Source::Count)> g_Targets{};
	inline std::array<std::atomic<ULONGLONG>, static_cast<size_t>(Source::Count)> g_Expirations{};

	inline void Mark(Source source, CPed* ped, ULONGLONG durationMs = 80)
	{
		if (!g_Config.ESP->Targets)
			return;

		const size_t index = static_cast<size_t>(source);
		if (!ped)
		{
			g_Targets[index].store(nullptr, std::memory_order_relaxed);
			g_Expirations[index].store(0, std::memory_order_relaxed);
			return;
		}

		g_Targets[index].store(ped, std::memory_order_relaxed);
		g_Expirations[index].store(GetTickCount64() + durationMs, std::memory_order_relaxed);
	}

	inline void Clear(Source source)
	{
		Mark(source, nullptr, 0);
	}

	inline bool IsPedHighlighted(CPed* ped)
	{
		if (!ped)
			return false;

		const ULONGLONG now = GetTickCount64();
		for (size_t i = 0; i < static_cast<size_t>(Source::Count); ++i)
		{
			const ULONGLONG expiration = g_Expirations[i].load(std::memory_order_relaxed);
			if (expiration <= now)
				continue;

			if (g_Targets[i].load(std::memory_order_relaxed) == ped)
				return true;
		}

		return false;
	}
}