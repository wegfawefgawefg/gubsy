#pragma once

class ModApiRegistry;
struct State;

void register_demo_items_api(ModApiRegistry& registry);
void finalize_demo_items_from_mods(State& state);
