#pragma once

#include "engine/menu/menu_screen.hpp"

#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

struct MenuCommandRegistry;

struct MenuManager {
    struct ScreenInstance {
        const MenuScreenDef* def{nullptr};
        void* state_ptr{nullptr};
        std::uint32_t state_size{0};
        std::uint32_t state_align{0};
    };

    MenuManager();
    MenuManager(const MenuManager&) = delete;
    MenuManager& operator=(const MenuManager&) = delete;
    ~MenuManager();

    void register_screen(const MenuScreenDef& def);
    const MenuScreenDef* find_screen(MenuScreenId id) const;

    bool push_screen(MenuScreenId id);
    void pop_screen();
    void clear();

    std::span<const ScreenInstance> stack() const { return stack_; }
    std::span<ScreenInstance> stack() { return stack_; }

    void set_command_registry(MenuCommandRegistry* registry) { commands_ = registry; }
    MenuCommandRegistry* commands() const { return commands_; }

private:
    void destroy_instance(ScreenInstance& inst);
    void create_instance(ScreenInstance& inst);

    std::unordered_map<MenuScreenId, MenuScreenDef> registry_;
    std::vector<ScreenInstance> stack_;
    MenuCommandRegistry* commands_{nullptr};
};

void init_menu_system();
void shutdown_menu_system();
