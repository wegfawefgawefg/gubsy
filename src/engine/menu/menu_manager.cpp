#include "engine/menu/menu_manager.hpp"

#include <algorithm>
#include <new>

namespace {
std::size_t normalize_align(std::size_t value) {
    if (value == 0)
        return alignof(std::max_align_t);
    return value;
}
} // namespace

MenuManager::MenuManager() = default;

MenuManager::~MenuManager() {
    clear();
}

void MenuManager::register_screen(const MenuScreenDef& def) {
    if (def.id == kMenuIdInvalid)
        return;
    registry_[def.id] = def;
}

const MenuScreenDef* MenuManager::find_screen(MenuScreenId id) const {
    auto it = registry_.find(id);
    if (it == registry_.end())
        return nullptr;
    return &it->second;
}

bool MenuManager::push_screen(MenuScreenId id) {
    const MenuScreenDef* def = find_screen(id);
    if (!def)
        return false;
    ScreenInstance inst;
    inst.def = def;
    inst.state_size = def->state_ops.size;
    inst.state_align = def->state_ops.align;
    create_instance(inst);
    stack_.push_back(inst);
    return true;
}

void MenuManager::pop_screen() {
    if (stack_.empty())
        return;
    ScreenInstance inst = stack_.back();
    stack_.pop_back();
    destroy_instance(inst);
}

void MenuManager::clear() {
    while (!stack_.empty()) {
        destroy_instance(stack_.back());
        stack_.pop_back();
    }
}

void MenuManager::create_instance(ScreenInstance& inst) {
    if (!inst.def)
        return;
    const ScreenStateOps& ops = inst.def->state_ops;
    if (!ops.size || !ops.init)
        return;
    std::size_t align = normalize_align(ops.align);
    inst.state_align = static_cast<std::uint32_t>(align);
    inst.state_size = ops.size;
    inst.state_ptr = ::operator new(static_cast<std::size_t>(inst.state_size),
                                    std::align_val_t(align));
    ops.init(inst.state_ptr);
}

void MenuManager::destroy_instance(ScreenInstance& inst) {
    if (!inst.def || !inst.state_ptr)
        return;
    const ScreenStateOps& ops = inst.def->state_ops;
    if (ops.destroy)
        ops.destroy(inst.state_ptr);
    std::size_t align = normalize_align(inst.state_align);
    ::operator delete(inst.state_ptr, std::align_val_t(align));
    inst.state_ptr = nullptr;
    inst.state_size = 0;
    inst.state_align = 0;
}

void init_menu_system() {}
void shutdown_menu_system() {}
