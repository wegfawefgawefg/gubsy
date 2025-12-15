

bool cleanup_gubsy() {
    save_audio_settings_to_ini("config/audio.ini");
    cleanup_mods_manager();
    cleanup_audio();
    cleanup_engine_state();
    cleanup_graphics();
    SDL_Quit();
    return true;
}