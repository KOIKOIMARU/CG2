#pragma once
#include "engine/base/ImGuiManager.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

// 戦闘HUD共通の文字・色・寸法。画面ごとに影や縮尺の流儀を増やさない。
namespace CombatHud {
inline constexpr ImU32 White = IM_COL32(241, 240, 235, 255);
inline constexpr ImU32 Muted = IM_COL32(184, 184, 180, 255);
inline constexpr ImU32 Blue = IM_COL32(91, 206, 242, 255);
inline constexpr ImU32 Gold = IM_COL32(231, 189, 109, 255);
inline constexpr ImU32 Danger = IM_COL32(237, 100, 88, 255);
inline constexpr ImU32 Track = IM_COL32(41, 42, 43, 220);
// 戦闘計器のアクセント。通常は金、危険だけ赤、虹はフィーバー発動中に限定する。
inline constexpr ImU32 GaugeGold = IM_COL32(255, 202, 70, 255);

inline float Scale(const Math::Vector2& viewport)
{
    return std::clamp((std::min)(viewport.x / 1280.0f, viewport.y / 720.0f), 0.5f, 2.0f);
}

inline ImFont* Font(bool number = false)
{
    return number ? ImGuiManager::GetHudNumberFont() : ImGuiManager::GetHudFont();
}

inline float Width(const char* text, float size, bool number = false)
{
    return Font(number)->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

inline void Text(ImDrawList* draw, ImVec2 position, float size, ImU32 color,
    const char* text, bool right = false, bool number = false)
{
    if (right) { position.x -= Width(text, size, number); }
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    const int alpha = static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xff);
    // 太さは書体で確保し、何重もの縁取りで小さな文字を潰さない。
    draw->AddText(Font(number), size, { position.x, position.y + 1.0f },
        IM_COL32(10, 11, 12, alpha * 3 / 4), text);
    draw->AddText(Font(number), size, position, color, text);
}

inline void Shade(ImDrawList* draw, ImVec2 min, ImVec2 max, bool right = false)
{
    const ImU32 dark = IM_COL32(12, 13, 14, 180);
    const ImU32 clear = IM_COL32(12, 13, 14, 0);
    draw->AddRectFilledMultiColor(min, max, right ? clear : dark, right ? dark : clear,
        right ? dark : clear, right ? clear : dark);
}

// 数字も和文も同じ書体・同じ縁取りで描く。タイトルのロゴ用書体は変更しない。
inline void Readout(ImDrawList* draw, ImVec2 position, float size, ImU32 color,
    const char* text, bool right = false)
{
    if (right) { position.x -= Width(text, size); }
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    const int alpha = static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xff);
    const ImU32 outline = IM_COL32(12, 14, 18, alpha * 4 / 5);
    for (const ImVec2 offset : { ImVec2(-1, 0), ImVec2(1, 0), ImVec2(0, -1), ImVec2(0, 1) }) {
        draw->AddText(Font(), size, { position.x + offset.x, position.y + offset.y }, outline, text);
    }
    draw->AddText(Font(), size, position, color, text);
}

// 細い金属枠と上面のハイライトを持つ計器。微小な残量にもクリップ矩形を使わない。
inline void Meter(ImDrawList* draw, ImVec2 min, ImVec2 max, float rate, ImU32 color, float scale)
{
    draw->AddRectFilled({ min.x - scale, min.y - scale }, { max.x + scale, max.y + scale }, IM_COL32(9, 12, 16, 240));
    draw->AddRectFilled(min, max, IM_COL32(25, 29, 34, 230));
    draw->AddRect(min, max, IM_COL32(179, 187, 194, 240), 0.0f, 0, scale);
    const ImVec2 inner(min.x + 2.0f * scale, min.y + 2.0f * scale);
    const ImVec2 end(max.x - 2.0f * scale, max.y - 2.0f * scale);
    rate = std::clamp(rate, 0.0f, 1.0f);
    if (rate <= 0.0f || end.x <= inner.x || end.y <= inner.y) { return; }
    const float edge = inner.x + (end.x - inner.x) * rate;
    draw->AddRectFilled(inner, { edge, end.y }, color);
    draw->AddRectFilledMultiColor(inner, { edge, end.y }, IM_COL32(255, 255, 255, 75),
        IM_COL32(255, 255, 255, 75), IM_COL32(0, 0, 0, 58), IM_COL32(0, 0, 0, 58));
    draw->AddLine({ inner.x, inner.y }, { edge, inner.y }, IM_COL32(255, 246, 216, 180), scale);
}

inline void ShipIcon(ImDrawList* draw, ImVec2 center, float scale, ImU32 color)
{
    // 自機の正面シルエット。架空の残機数を表示せず、体力の所属だけを伝える。
    draw->AddTriangleFilled({ center.x, center.y - 17 * scale },
        { center.x + 5 * scale, center.y + 13 * scale }, { center.x - 5 * scale, center.y + 13 * scale }, color);
    for (const float side : { -1.0f, 1.0f }) {
        draw->AddTriangleFilled({ center.x + side * 4 * scale, center.y - 3 * scale },
            { center.x + side * 23 * scale, center.y + 9 * scale },
            { center.x + side * 5 * scale, center.y + 8 * scale }, color);
    }
}

inline void Segments(ImDrawList* draw, ImVec2 min, ImVec2 max,
    float rate, ImU32 color, int count, float gap)
{
    const float width = (max.x - min.x - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    for (int index = 0; index < count; ++index) {
        const float x = min.x + static_cast<float>(index) * (width + gap);
        draw->AddRectFilled({ x, min.y }, { x + width, max.y }, Track);
        const float filled = std::clamp(rate * static_cast<float>(count) - static_cast<float>(index), 0.0f, 1.0f);
        if (filled > 0.0f) { draw->AddRectFilled({ x, min.y }, { x + width * filled, max.y }, color); }
    }
}

inline void BladeIcon(ImDrawList* draw, ImVec2 center, float scale, ImU32 color)
{
    // 二本の刃を機体の翼と同じ鋭い形で描く。文字ではなく技を識別する目印。
    for (const float offset : { -7.0f, 7.0f }) {
        const ImVec2 blade[] = {
            { center.x + (offset - 9.0f) * scale, center.y + 12.0f * scale },
            { center.x + (offset + 11.0f) * scale, center.y - 16.0f * scale },
            { center.x + (offset + 7.0f) * scale, center.y + 1.0f * scale },
            { center.x + (offset - 5.0f) * scale, center.y + 14.0f * scale }
        };
        draw->AddConvexPolyFilled(blade, 4, color);
        draw->AddLine({ center.x + (offset - 10.0f) * scale, center.y + 10.0f * scale },
            { center.x + (offset + 1.0f) * scale, center.y + 17.0f * scale }, color, 2.0f * scale);
    }
}
} // namespace CombatHud
