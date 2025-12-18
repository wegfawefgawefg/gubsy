#pragma once

namespace GameAction {
    inline constexpr int MENU_UP = 0;
    inline constexpr int MENU_DOWN = 1;
    inline constexpr int MENU_LEFT = 2;
    inline constexpr int MENU_RIGHT = 3;
    inline constexpr int MENU_SELECT = 4;
    inline constexpr int MENU_BACK = 5;
    inline constexpr int MENU_SCALE_UP = 6;
    inline constexpr int MENU_SCALE_DOWN = 7;

    inline constexpr int UP  = 10;
    inline constexpr int DOWN = 11;
    inline constexpr int LEFT = 12;
    inline constexpr int RIGHT = 13;
    inline constexpr int USE = 14;
    inline constexpr std::size_t COUNT = 18;
}

namespace GameAnalog1D {
    inline constexpr int BAR_HEIGHT = 0;
    inline constexpr std::size_t COUNT = 1;
}

namespace GameAnalog2D {
    inline constexpr int RETICLE_POS = 0;
    inline constexpr std::size_t COUNT = 1;
}
