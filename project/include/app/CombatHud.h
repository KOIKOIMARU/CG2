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
// 機体のマーキングとエネルギーを別の色で識別する。タイトルの配色は変えない。
inline constexpr ImU32 Vermilion = IM_COL32(255, 83, 49, 255);
inline constexpr ImU32 Energy = IM_COL32(225, 250, 91, 255);
inline constexpr ImU32 Ink = IM_COL32(24, 23, 30, 245);
inline constexpr ImU32 Fever = IM_COL32(209, 128, 255, 255);

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

// 翼・斬撃と同じ傾きを持つ面。枠線ではなく色面の輪郭でHUDを識別する。
inline void CutPlate(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 color, float cut)
{
    const ImVec2 points[] = {
        { min.x + cut, min.y }, { max.x, min.y },
        { max.x - cut, max.y }, { min.x, max.y }
    };
    draw->AddConvexPolyFilled(points, 4, color);
}

inline void CutMeter(ImDrawList* draw, ImVec2 min, ImVec2 max, float rate, ImU32 color, float cut)
{
    CutPlate(draw, min, max, Ink, cut);
    rate = std::clamp(rate, 0.0f, 1.0f);
    if (rate <= 0.0f) { return; }
    const float edge = min.x + (max.x - min.x) * rate;
    const ImVec2 plate[] = {
        { min.x + cut, min.y }, { max.x, min.y },
        { max.x - cut, max.y }, { min.x, max.y }
    };
    // 1px未満のクリップ矩形はD3D12側で空になるため、塗る多角形をCPUで切り出す。
    ImVec2 filled[6]{};
    int count = 0;
    ImVec2 previous = plate[3];
    for (const ImVec2 current : plate) {
        const bool previousInside = previous.x <= edge;
        const bool currentInside = current.x <= edge;
        if (previousInside != currentInside) {
            const float t = (edge - previous.x) / (current.x - previous.x);
            filled[count++] = { edge, previous.y + (current.y - previous.y) * t };
        }
        if (currentInside) { filled[count++] = current; }
        previous = current;
    }
    if (count >= 3) { draw->AddConvexPolyFilled(filled, count, color); }
}

// 英数字を前傾させ、和文の可読性は保つ。既存フォントを再利用し追加画像は不要。
inline void SpeedText(ImDrawList* draw, ImVec2 position, float size, ImU32 color,
    const char* text, bool right = false)
{
    if (right) { position.x -= size * 0.12f; }
    const int first = draw->VtxBuffer.Size;
    Text(draw, position, size, color, text, right, true);
    for (int index = first; index < draw->VtxBuffer.Size; ++index) {
        auto& vertex = draw->VtxBuffer[index];
        vertex.pos.x += (position.y + size - vertex.pos.y) * 0.12f;
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
