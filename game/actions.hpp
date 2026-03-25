#pragma once

namespace GameAction {
inline constexpr int MENU_UP = 0;
inline constexpr int MENU_DOWN = 1;
inline constexpr int MENU_LEFT = 2;
inline constexpr int MENU_RIGHT = 3;
inline constexpr int MENU_SELECT = 4;
inline constexpr int MENU_BACK = 5;
inline constexpr int MENU_PAGE_NEXT = 6;
inline constexpr int MENU_PAGE_PREV = 7;

inline constexpr int UP  = 8;
inline constexpr int DOWN = 9;
inline constexpr int LEFT = 10;
inline constexpr int RIGHT = 11;
inline constexpr int USE = 12;
inline constexpr std::size_t COUNT = 13;
}

namespace GameAnalog1D {
    inline constexpr int BAR_HEIGHT = 0;
    inline constexpr std::size_t COUNT = 1;
}

namespace GameAnalog2D {
    inline constexpr int RETICLE_POS = 0;
    inline constexpr std::size_t COUNT = 1;
}
