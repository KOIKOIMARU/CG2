#pragma once
#include "app/CombatHud.h"

// タイトル・操作説明・リザルトの共通部品。戦闘HUDと同じ書体で組む。
namespace MenuUi {
inline constexpr ImU32 Ink = IM_COL32(17, 22, 30, 255);
inline constexpr ImU32 Paper = IM_COL32(239, 241, 244, 255);
inline constexpr ImU32 Quiet = IM_COL32(156, 168, 186, 255);
inline constexpr ImU32 Accent = IM_COL32(224, 91, 68, 255);

inline float Width(const char* text, float size) { return CombatHud::ReadoutWidth(text, size * 1.25f); }

inline void Text(ImDrawList* draw, ImVec2 at, float size, ImU32 color, const char* text, bool right = false)
{
    if (right) { at.x -= Width(text, size); }
    draw->AddText(CombatHud::BattleFont(), size * 1.25f, { std::round(at.x), std::round(at.y) },
        CombatHud::SurfaceColor(color), text);
}

inline void Sheet(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale)
{
    draw->AddRectFilled({ min.x + 8 * scale, min.y + 12 * scale },
        { max.x + 8 * scale, max.y + 12 * scale }, IM_COL32(0, 0, 0, 70));
    const ImU32 top = CombatHud::SurfaceColor(IM_COL32(34, 43, 57, 252));
    const ImU32 bottom = CombatHud::SurfaceColor(IM_COL32(15, 21, 31, 252));
    draw->AddRectFilledMultiColor(min, max, top, top, bottom, bottom);
    draw->AddLine(min, { max.x, min.y }, CombatHud::SurfaceColor(IM_COL32(126, 142, 164, 180)), scale);
}

// 選択矢印や下線は付けず、面の明暗で押せる場所を示す。
inline bool Button(ImDrawList* draw, const char* id, const char* label, ImVec2 min, ImVec2 size,
    float scale, bool selected = false)
{
    ImGui::SetCursorScreenPos(min);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool lit = selected || ImGui::IsItemHovered();
    const ImVec2 max{ min.x + size.x, min.y + size.y };
    const ImU32 top = lit ? Paper : IM_COL32(42, 51, 65, 255);
    const ImU32 bottom = lit ? IM_COL32(197, 207, 221, 255) : IM_COL32(26, 34, 47, 255);
    draw->AddRectFilledMultiColor(min, max, CombatHud::SurfaceColor(top), CombatHud::SurfaceColor(top),
        CombatHud::SurfaceColor(bottom), CombatHud::SurfaceColor(bottom));
    if (!lit) { draw->AddRect(min, max, CombatHud::SurfaceColor(IM_COL32(89, 105, 126, 180)), 0, 0, scale); }
    const float fontSize = 21.0f * scale;
    Text(draw, { min.x + (size.x - Width(label, fontSize)) * 0.5f,
        min.y + (size.y - fontSize * 1.25f) * 0.5f - scale }, fontSize, lit ? Ink : Paper, label);
    return clicked;
}

inline void Key(ImDrawList* draw, ImVec2 min, float width, float scale, const char* label)
{
    const ImVec2 max{ min.x + width * scale, min.y + 36 * scale };
    draw->AddRectFilled({ min.x, min.y + 3 * scale }, { max.x, max.y + 3 * scale },
        CombatHud::SurfaceColor(IM_COL32(9, 12, 18, 255)), 3 * scale);
    draw->AddRectFilled(min, max, CombatHud::SurfaceColor(IM_COL32(57, 68, 86, 255)), 3 * scale);
    draw->AddRect(min, max, CombatHud::SurfaceColor(IM_COL32(123, 141, 164, 200)), 3 * scale, 0, scale);
    Text(draw, { min.x + (width * scale - Width(label, 21 * scale)) * 0.5f,
        min.y + 3 * scale }, 21 * scale, Paper, label);
}

// 同じ説明をタイトルと本編で共有。キーと動作の対応を一画面に置く。
inline bool Controls(ImVec2 viewportMin, ImVec2 viewportSize, bool inGame)
{
    const float scale = CombatHud::Scale({ viewportSize.x, viewportSize.y });
    const ImVec2 origin{ viewportMin.x + (viewportSize.x - 980 * scale) * 0.5f,
        viewportMin.y + (viewportSize.y - 560 * scale) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    ImGui::SetNextWindowPos(viewportMin);
    ImGui::SetNextWindowSize(viewportSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##ControlSheet", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(viewportMin, { viewportMin.x + viewportSize.x, viewportMin.y + viewportSize.y },
        IM_COL32(0, 0, 0, 185));
    Sheet(draw, origin, p(980, 560), scale);
    Text(draw, p(48, 32), 36 * scale, Paper, "操作方法");
    draw->AddLine(p(470, 122), p(470, 406), CombatHud::SurfaceColor(IM_COL32(77, 91, 111, 150)), scale);
    Text(draw, p(48, 120), 23 * scale, Paper, "移動");
    Key(draw, p(205, 112), 40, scale, "W");
    Key(draw, p(159, 154), 40, scale, "A");
    Key(draw, p(205, 154), 40, scale, "S");
    Key(draw, p(251, 154), 40, scale, "D");
    Text(draw, p(159, 207), 17 * scale, Quiet, "方向キーでも操作");
    Text(draw, p(48, 269), 23 * scale, Paper, "照準");
    Text(draw, p(159, 269), 23 * scale, Paper, "マウス");
    Text(draw, p(48, 350), 23 * scale, Paper, "回避");
    Key(draw, p(159, 344), 68, scale, "A / D");
    Text(draw, p(240, 350), 23 * scale, Quiet, "+");
    Key(draw, p(266, 344), 88, scale, "SHIFT");
    Text(draw, p(512, 120), 23 * scale, Paper, "射撃");
    Key(draw, p(790, 112), 142, scale, "SPACE");
    Text(draw, p(512, 161), 18 * scale, Quiet, "長押しで連射");
    Text(draw, p(512, 230), 23 * scale, Paper, "チャージ");
    Text(draw, p(512, 271), 18 * scale, Quiet, "SPACEを離してため、次の一発を強化");
    Text(draw, p(512, 350), 23 * scale, Paper, "残像連撃");
    Key(draw, p(890, 344), 42, scale, "Q");
    Text(draw, p(512, 391), 18 * scale, Quiet, "8秒で回復・射撃の命中で短縮");
    draw->AddLine(p(48, 444), p(932, 444), CombatHud::SurfaceColor(IM_COL32(77, 91, 111, 150)), scale);
    Text(draw, p(48, 465), 18 * scale, Quiet, "フィーバーはゲージ満タンで自動発動");
    if (inGame) { Text(draw, p(48, 504), 15 * scale, Quiet, "表示中もゲームは進行します"); }
    const bool close = Button(draw, "close_controls", "戻る", p(754, 488), { 178 * scale, 44 * scale }, scale);
    ImGui::End();
    ImGui::PopStyleVar();
    return close;
}
} // namespace MenuUi
