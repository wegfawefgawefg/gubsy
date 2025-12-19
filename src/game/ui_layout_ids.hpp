#pragma once

namespace UILayoutID {
inline constexpr int PLAY_SCREEN = 1;
inline constexpr int MENU_SCREEN = 2;
inline constexpr int MODS_SCREEN = 3;
} // namespace UILayoutID

namespace UIObjectID {
// Play screen objects
inline constexpr int HEALTHBAR = 1;
inline constexpr int INVENTORY = 2;
inline constexpr int MINIMAP = 3;
inline constexpr int BAR_HEIGHT_INDICATOR = 4;
inline constexpr int RETICLE = 5;
} // namespace UIObjectID

namespace MenuObjectID {
inline constexpr int TITLE = 101;
inline constexpr int PLAY = 102;
inline constexpr int MODS = 103;
inline constexpr int SETTINGS = 104;
inline constexpr int QUIT = 105;
} // namespace MenuObjectID

namespace ModsObjectID {
inline constexpr int TITLE = 301;
inline constexpr int STATUS = 302;
inline constexpr int SEARCH = 303;
inline constexpr int PREV = 304;
inline constexpr int NEXT = 305;
inline constexpr int PAGE = 306;
inline constexpr int CARD0 = 320;
inline constexpr int CARD1 = 321;
inline constexpr int CARD2 = 322;
inline constexpr int CARD3 = 323;
inline constexpr int BACK = 330;
} // namespace ModsObjectID
