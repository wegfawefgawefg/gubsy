#pragma once

constexpr const char* DATA_FOLDER_PATH = "data";

/* function to create the data folder and its internal layout if not exist
    binds profiles
    player profiles
    saves
    settings_profiles
*/
void ensure_data_folder_structure();