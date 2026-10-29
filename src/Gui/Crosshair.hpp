#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include <Core/Config.hpp>

namespace Gui::Crosshair {
	using custom_pixels_t = std::array<std::uint8_t, Core::Config::General::CrosshairCustomGridSize * Core::Config::General::CrosshairCustomGridSize>;

	inline ImVec2 SnapShapeCenter(const ImVec2& point) {
		return ImVec2{ std::floor(point.x) + 0.5f, std::floor(point.y) + 0.5f };
	}

	inline constexpr int kStyleSkriptGG = 0;
	inline constexpr int kStyleDot = 1;
	inline constexpr int kStylePlus = 2;
	inline constexpr int kStyleCross = 3;
	inline constexpr int kStyleBox = 4;
	inline constexpr int kStyleCustom = 5;
	inline constexpr int kStyleCount = 6;

	inline constexpr std::array<const char*, kStyleCount> kStyleLabels{
		"skript.gg",
		"Dot",
		"Plus",
		"Cross",
		"Box",
		"Custom"
	};

	inline int g_RuntimeStyleOverride = -1;

	inline int NormalizeStyle(const int style) {
		return ImClamp(style, 0, kStyleCount - 1);
	}

	inline int GetActiveStyle(const int configured_style) {
		return NormalizeStyle(g_RuntimeStyleOverride >= 0 ? g_RuntimeStyleOverride : configured_style);
	}

	inline void SetStyleOverride(const int style) {
		g_RuntimeStyleOverride = NormalizeStyle(style);
	}

	inline constexpr ImU32 kFallbackMainColor = IM_COL32(205, 50, 65, 255);
	inline constexpr ImU32 kFallbackAccentColor = IM_COL32(128, 0, 128, 255);

	inline bool HasVisibleAlpha(const ImU32 color) {
		return ((color >> IM_COL32_A_SHIFT) & 0xFF) > 3;
	}

	inline ImU32 EnsureVisibleColor(const ImU32 color, const ImU32 fallback) {
		return HasVisibleAlpha(color) ? color : fallback;
	}

	struct custom_bounds_t {
		int min_row = 0;
		int max_row = 0;
		int min_col = 0;
		int max_col = 0;
	};

	inline custom_bounds_t GetCustomBounds(const custom_pixels_t& pixels) {
		custom_bounds_t bounds{
			Core::Config::General::CrosshairCustomGridSize,
			-1,
			Core::Config::General::CrosshairCustomGridSize,
			-1
		};

		for (int row = 0; row < Core::Config::General::CrosshairCustomGridSize; ++row) {
			for (int col = 0; col < Core::Config::General::CrosshairCustomGridSize; ++col) {
				const std::size_t index = static_cast<std::size_t>(row * Core::Config::General::CrosshairCustomGridSize + col);
				if (!pixels[index]) {
					continue;
				}

				bounds.min_row = ImMin(bounds.min_row, row);
				bounds.max_row = ImMax(bounds.max_row, row);
				bounds.min_col = ImMin(bounds.min_col, col);
				bounds.max_col = ImMax(bounds.max_col, col);
			}
		}

		if (bounds.max_row < bounds.min_row || bounds.max_col < bounds.min_col) {
			constexpr int center = Core::Config::General::CrosshairCustomGridSize / 2;
			bounds.min_row = center;
			bounds.max_row = center;
			bounds.min_col = center;
			bounds.max_col = center;
		}

		return bounds;
	}

	inline bool HasAnyCustomPixels(const custom_pixels_t& pixels) {
		for (const std::uint8_t pixel : pixels) {
			if (pixel) {
				return true;
			}
		}

		return false;
	}

	inline float GetCustomExpansion(const float cell, const float thickness) {
		return ImMax(0.0f, (thickness - 1.0f) * cell * 0.22f);
	}

	inline void DrawSegment(ImDrawList* draw_list, const ImVec2& from, const ImVec2& to, const float thickness, const float roundness, const ImU32 color) {
		const ImVec2 delta = to - from;
		const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
		if (length <= 0.001f) {
			const float radius = ImMax(0.5f, thickness * 0.5f);
			draw_list->AddCircleFilled(from, radius, color, 24);
			return;
		}

		const float radius = ImMax(0.5f, thickness * 0.5f);
		const float cap = ImMin(radius * ImClamp(roundness, 0.0f, 1.0f), length * 0.5f);
		const ImVec2 dir = delta / length;
		const ImVec2 normal{ -dir.y * radius, dir.x * radius };
		const ImVec2 body_start = from + dir * cap;
		const ImVec2 body_end = to - dir * cap;

		draw_list->AddQuadFilled(
			body_start + normal,
			body_end + normal,
			body_end - normal,
			body_start - normal,
			color
		);

		if (cap > 0.001f) {
			draw_list->AddCircleFilled(body_start, cap, color, 24);
			draw_list->AddCircleFilled(body_end, cap, color, 24);
		}
	}

	inline void DrawArc(ImDrawList* draw_list, const ImVec2& center, const float radius, const float angle_min, const float angle_max, const float thickness, const float roundness, const ImU32 color) {
		const int segments = 24;
		for (int i = 0; i < segments; ++i) {
			const float t0 = static_cast<float>(i) / static_cast<float>(segments);
			const float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
			const float a0 = ImLerp(angle_min, angle_max, t0);
			const float a1 = ImLerp(angle_min, angle_max, t1);
			const ImVec2 p0{ center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius };
			const ImVec2 p1{ center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius };
			DrawSegment(draw_list, p0, p1, thickness, roundness, color);
		}
	}

	inline ImVec2 MeasureHalfExtent(const int style, const float size, const float thickness, const custom_pixels_t& pixels) {
		const float safe_size = ImMax(size, 0.5f);
		const float safe_thickness = ImMax(thickness, 1.0f);

		switch (GetActiveStyle(style)) {
		case kStyleDot: {
			const float radius = ImMax(1.0f, 1.25f * safe_size);
			return { radius, radius };
		}
		case kStylePlus: {
			const float gap = ImMax(1.0f, 2.0f * safe_size);
			const float arm = ImMax(3.0f, 5.0f * safe_size);
			const float extent = gap + arm + safe_thickness * 0.5f;
			return { extent, extent };
		}
		case kStyleCross: {
			const float gap = ImMax(1.0f, 1.6f * safe_size);
			const float arm = ImMax(3.0f, 4.6f * safe_size);
			const float outer = (gap + arm) * 0.70710678f;
			const float extent = outer + safe_thickness * 0.5f;
			return { extent, extent };
		}
		case kStyleBox: {
			const float half = ImMax(4.0f, 5.5f * safe_size);
			const float extent = half + safe_thickness * 0.5f;
			return { extent, extent };
		}
		case kStyleCustom: {
			const float cell = ImMax(1.5f, std::round(safe_size * 1.8f));
			const auto bounds = GetCustomBounds(pixels);
			const float width = (bounds.max_col - bounds.min_col + 1) * cell;
			const float height = (bounds.max_row - bounds.min_row + 1) * cell;
			const float expansion = GetCustomExpansion(cell, safe_thickness);
			return { width * 0.5f + expansion, height * 0.5f + expansion };
		}
		case kStyleSkriptGG:
		default: {
			const float radius = 4.0f * safe_size;
			const float line_length = 3.0f * safe_size;
			return { radius + line_length + safe_thickness * 0.5f, safe_size + radius + safe_thickness * 0.5f };
		}
		}
	}

	inline void DrawDot(ImDrawList* draw_list, const ImVec2& center, const float size, const float roundness, const ImU32 color) {
		const float radius = ImMax(1.0f, 1.25f * size);
		const float rounding = ImClamp(roundness, 0.0f, 1.0f) * radius;
		draw_list->AddRectFilled(center - ImVec2{ radius, radius }, center + ImVec2{ radius, radius }, color, rounding);
	}

	inline void DrawPlus(ImDrawList* draw_list, const ImVec2& center, const float size, const float thickness, const float roundness, const ImU32 color) {
		const float gap = ImMax(1.0f, 2.0f * size);
		const float arm = ImMax(3.0f, 5.0f * size);

		DrawSegment(draw_list, { center.x - gap - arm, center.y }, { center.x - gap, center.y }, thickness, roundness, color);
		DrawSegment(draw_list, { center.x + gap, center.y }, { center.x + gap + arm, center.y }, thickness, roundness, color);
		DrawSegment(draw_list, { center.x, center.y - gap - arm }, { center.x, center.y - gap }, thickness, roundness, color);
		DrawSegment(draw_list, { center.x, center.y + gap }, { center.x, center.y + gap + arm }, thickness, roundness, color);
	}

	inline void DrawCross(ImDrawList* draw_list, const ImVec2& center, const float size, const float thickness, const float roundness, const ImU32 color) {
		const float gap = ImMax(1.0f, 1.6f * size);
		const float arm = ImMax(3.0f, 4.6f * size);
		const float inner = gap * 0.70710678f;
		const float outer = (gap + arm) * 0.70710678f;

		DrawSegment(draw_list, { center.x - outer, center.y - outer }, { center.x - inner, center.y - inner }, thickness, roundness, color);
		DrawSegment(draw_list, { center.x + inner, center.y + inner }, { center.x + outer, center.y + outer }, thickness, roundness, color);
		DrawSegment(draw_list, { center.x - outer, center.y + outer }, { center.x - inner, center.y + inner }, thickness, roundness, color);
		DrawSegment(draw_list, { center.x + inner, center.y - inner }, { center.x + outer, center.y - outer }, thickness, roundness, color);
	}

	inline void DrawBox(ImDrawList* draw_list, const ImVec2& center, const float size, const float thickness, const float roundness, const ImU32 color, const ImU32 accent_color) {
		const float half = ImMax(4.0f, 5.5f * size);
		const float rounding = ImClamp(roundness, 0.0f, 1.0f) * half;
		draw_list->AddRect({ center.x - half, center.y - half }, { center.x + half, center.y + half }, color, rounding, 0, thickness);
		DrawDot(draw_list, center, size * 0.7f, roundness, accent_color);
	}

	inline void DrawSkriptGG(ImDrawList* draw_list, const ImVec2& center, const float size, const float thickness, const float roundness, const ImU32 dot_color, const ImU32 accent_color) {
		(void)roundness;
		(void)dot_color;
		(void)accent_color;

		const ImVec2 snapped_center = SnapShapeCenter(center);
		const ImU32 skript_dot_color = IM_COL32(205, 50, 65, 255);
		const ImU32 skript_arc_color = IM_COL32(128, 0, 128, 255);
		const float dot_radius = 1.0f * size;
		const int dot_segments = ImClamp(static_cast<int>(std::round(dot_radius * 14.0f)), 32, 64);
		draw_list->AddCircleFilled(snapped_center, dot_radius, skript_dot_color, dot_segments);

		const float radius = 4.0f * size;
		const ImVec2 arc_center = SnapShapeCenter(ImVec2{ snapped_center.x, snapped_center.y + (1.0f * size) });
		const int arc_segments = ImClamp(static_cast<int>(std::round(radius * 10.0f)), 28, 72);
		draw_list->PathArcTo(arc_center, radius, 0.0f, IM_PI, arc_segments);
		draw_list->PathStroke(skript_arc_color, 0, 1.0f * thickness);

		const float line_length = 3.0f * size;
		draw_list->AddLine(
			{ arc_center.x - radius - line_length, arc_center.y },
			{ arc_center.x - radius, arc_center.y },
			skript_arc_color,
			1.0f * thickness
		);
		draw_list->AddLine(
			{ arc_center.x + radius, arc_center.y },
			{ arc_center.x + radius + line_length, arc_center.y },
			skript_arc_color,
			1.0f * thickness
		);
	}

	inline void DrawCustom(ImDrawList* draw_list, const ImVec2& center, const float size, const float thickness, const float roundness, const ImU32 color, const custom_pixels_t& pixels) {
		const float cell = ImMax(1.5f, std::round(size * 1.8f));
		const auto bounds = GetCustomBounds(pixels);
		const float width = (bounds.max_col - bounds.min_col + 1) * cell;
		const float height = (bounds.max_row - bounds.min_row + 1) * cell;
		const ImVec2 origin{
			std::round(center.x - width * 0.5f),
			std::round(center.y - height * 0.5f)
		};
		const float expansion = GetCustomExpansion(cell, thickness);
		const float rounding = ImClamp(roundness, 0.0f, 1.0f) * (cell * 0.5f + expansion);

		for (int row = bounds.min_row; row <= bounds.max_row; ++row) {
			int col = bounds.min_col;
			while (col <= bounds.max_col) {
				const std::size_t index = static_cast<std::size_t>(row * Core::Config::General::CrosshairCustomGridSize + col);
				if (!pixels[index]) {
					++col;
					continue;
				}

				int run_end = col + 1;
				while (run_end <= bounds.max_col) {
					const std::size_t run_index = static_cast<std::size_t>(row * Core::Config::General::CrosshairCustomGridSize + run_end);
					if (!pixels[run_index]) {
						break;
					}
					++run_end;
				}

				const ImVec2 rect_min = origin + ImVec2{ (col - bounds.min_col) * cell, (row - bounds.min_row) * cell };
				const ImVec2 rect_max = rect_min + ImVec2{ (run_end - col) * cell, cell };
				draw_list->AddRectFilled(rect_min - ImVec2{ expansion, expansion }, rect_max + ImVec2{ expansion, expansion }, color, rounding);
				col = run_end;
			}
		}
	}

	inline void Draw(ImDrawList* draw_list, const ImVec2& center, const int style, float size, float thickness, float roundness, const ImU32 main_color, const ImU32 accent_color, const custom_pixels_t& pixels) {
		if (!draw_list) {
			return;
		}

		size = ImMax(size, 0.5f);
		thickness = ImMax(thickness, 1.0f);
		const ImU32 visible_main_color = EnsureVisibleColor(main_color, kFallbackMainColor);
		const ImU32 visible_accent_color = EnsureVisibleColor(accent_color, kFallbackAccentColor);

		switch (GetActiveStyle(style)) {
		case kStyleDot:
			DrawDot(draw_list, center, size, roundness, visible_main_color);
			break;
		case kStylePlus:
			DrawPlus(draw_list, center, size, thickness, roundness, visible_main_color);
			break;
		case kStyleCross:
			DrawCross(draw_list, center, size, thickness, roundness, visible_main_color);
			break;
		case kStyleBox:
			DrawBox(draw_list, center, size, thickness, roundness, visible_main_color, visible_accent_color);
			break;
		case kStyleCustom:
			if (HasAnyCustomPixels(pixels)) {
				DrawCustom(draw_list, center, size, thickness, roundness, visible_main_color, pixels);
			} else {
				DrawPlus(draw_list, center, size, thickness, roundness, visible_main_color);
			}
			break;
		case kStyleSkriptGG:
		default:
			DrawSkriptGG(draw_list, center, size, thickness, roundness, visible_main_color, visible_accent_color);
			break;
		}
	}
}
