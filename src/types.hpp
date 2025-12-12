#pragma once

#include <cstddef>
#include <cstdint>

// Integer IDs instead of enums (moddable via data tables later)
namespace ids {
inline constexpr const char* MODE_TITLE = "title";
inline constexpr const char* MODE_SETTINGS = "settings";
inline constexpr const char* MODE_VIDEO_SETTINGS = "video_settings";
inline constexpr const char* MODE_PLAYING = "playing";
inline constexpr const char* MODE_GAME_OVER = "game_over";
inline constexpr const char* MODE_WIN = "win";
inline constexpr const char* MODE_SCORE_REVIEW = "score_review";
inline constexpr const char* MODE_NEXT_STAGE = "next_stage";

inline constexpr int ET_NONE = 0;
inline constexpr int ET_PLAYER = 1;
inline constexpr int ET_NPC = 2;
inline constexpr int ET_ITEM = 3;
} // namespace ids

struct VID {
    std::size_t id{0};
    uint32_t version{0};
};
