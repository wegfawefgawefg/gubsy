#pragma once

namespace UILayoutID {
inline constexpr int PLAY_SCREEN = 1;
inline constexpr int MENU_SCREEN = 2;
inline constexpr int MODS_SCREEN = 3;
inline constexpr int SETTINGS_SCREEN = 4;
inline constexpr int LOBBY_SCREEN = 5;
inline constexpr int LOBBY_MODS_SCREEN = 6;
inline constexpr int SERVER_BROWSER_SCREEN = 7;
inline constexpr int GAME_SETTINGS_SCREEN = 8;
inline constexpr int LOCAL_PLAYERS_SCREEN = 9;
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

namespace SettingsObjectID {
inline constexpr int TITLE = 401;
inline constexpr int STATUS = 402;
inline constexpr int SEARCH = 403;
inline constexpr int PREV = 404;
inline constexpr int NEXT = 405;
inline constexpr int PAGE = 406;
inline constexpr int CARD0 = 420;
inline constexpr int CARD1 = 421;
inline constexpr int CARD2 = 422;
inline constexpr int CARD3 = 423;
inline constexpr int BACK = 430;
} // namespace SettingsObjectID

namespace LobbyObjectID {
inline constexpr int TITLE = 501;
inline constexpr int LOBBY_NAME = 502;
inline constexpr int PRIVACY = 503;
inline constexpr int GAME_SETTINGS = 504;
inline constexpr int MAX_PLAYERS = 505;
inline constexpr int LOCAL_PLAYERS = 506;
inline constexpr int SEED = 507;
inline constexpr int RANDOMIZE_SEED = 508;
inline constexpr int PLAYERS_PANEL = 509;
inline constexpr int MODS_PANEL = 510;
inline constexpr int MANAGE_MODS = 511;
inline constexpr int START_GAME = 512;
inline constexpr int BACK = 513;
inline constexpr int BROWSE_SERVERS = 514;
inline constexpr int PLAYER0 = 515;
inline constexpr int PLAYER1 = 516;
inline constexpr int PLAYER2 = 517;
inline constexpr int PLAYER3 = 518;
} // namespace LobbyObjectID

namespace LobbyModsObjectID {
inline constexpr int TITLE = 601;
inline constexpr int STATUS = 602;
inline constexpr int SEARCH = 603;
inline constexpr int PREV = 604;
inline constexpr int NEXT = 605;
inline constexpr int PAGE = 606;
inline constexpr int CARD0 = 620;
inline constexpr int CARD1 = 621;
inline constexpr int CARD2 = 622;
inline constexpr int CARD3 = 623;
inline constexpr int BACK = 630;
} // namespace LobbyModsObjectID

namespace ServerBrowserObjectID {
inline constexpr int TITLE = 701;
inline constexpr int STATUS = 702;
inline constexpr int CARD0 = 720;
inline constexpr int CARD1 = 721;
inline constexpr int CARD2 = 722;
inline constexpr int CARD3 = 723;
inline constexpr int BACK = 730;
} // namespace ServerBrowserObjectID

namespace GameSettingsObjectID {
inline constexpr int TITLE = 801;
inline constexpr int STATUS = 802;
inline constexpr int BACK = 830;
} // namespace GameSettingsObjectID

namespace LocalPlayersObjectID {
inline constexpr int TITLE = 901;
inline constexpr int STATUS = 902;
inline constexpr int PREV = 903;
inline constexpr int NEXT = 904;
inline constexpr int PAGE = 905;
inline constexpr int MORE = 906;
inline constexpr int LESS = 907;
inline constexpr int CARD0 = 920;
inline constexpr int CARD1 = 921;
inline constexpr int CARD2 = 922;
inline constexpr int CARD3 = 923;
inline constexpr int BACK = 930;
} // namespace LocalPlayersObjectID
