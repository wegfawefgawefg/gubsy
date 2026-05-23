#include "src/mod_server_config.hpp"

#include <cstdlib>
#include <string>

std::string default_mod_server_url() {
    if (const char* value = std::getenv("GUB_MOD_SERVER_URL")) {
        if (*value != '\0')
            return value;
    }
    return "http://127.0.0.1:8787";
}
