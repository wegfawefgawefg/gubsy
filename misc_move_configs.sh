#!/bin/bash
set -euo pipefail
mkdir -p data/{player_profiles,settings_profiles}
if [ -f config/user_profiles.sxp ]; then mv config/user_profiles.sxp data/player_profiles/; fi
if [ -f config/game_settings.sxp ]; then mv config/game_settings.sxp data/settings_profiles/; fi
if [ -f config/top_level_game_settings.sxp ]; then mv config/top_level_game_settings.sxp data/settings_profiles/; fi
if [ -f config/input_settings_profiles.sxp ]; then mv config/input_settings_profiles.sxp data/settings_profiles/; fi
if [ -f config/audio.ini ]; then mv config/audio.ini data/settings_profiles/; fi
if [ -f config/mods_enabled.json ]; then mv config/mods_enabled.json data/; fi
