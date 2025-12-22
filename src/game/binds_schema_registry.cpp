#include "game/binds_schema_registry.hpp"

#include "engine/binds_profiles.hpp"
#include "game/actions.hpp"

void register_binds_schema_entries() {
    BindsSchema schema;
    schema.add_action(GameAction::MENU_UP, "Move Up", "Menu");
    schema.add_action(GameAction::MENU_DOWN, "Move Down", "Menu");
    schema.add_action(GameAction::MENU_LEFT, "Move Left", "Menu");
    schema.add_action(GameAction::MENU_RIGHT, "Move Right", "Menu");
    schema.add_action(GameAction::MENU_SELECT, "Select", "Menu");
    schema.add_action(GameAction::MENU_BACK, "Back", "Menu");
    schema.add_action(GameAction::MENU_SCALE_UP, "Scale Up", "Menu");
    schema.add_action(GameAction::MENU_SCALE_DOWN, "Scale Down", "Menu");
    schema.add_action(GameAction::UP, "Move Up", "Movement");
    schema.add_action(GameAction::DOWN, "Move Down", "Movement");
    schema.add_action(GameAction::LEFT, "Move Left", "Movement");
    schema.add_action(GameAction::RIGHT, "Move Right", "Movement");
    schema.add_action(GameAction::USE, "Use/Interact", "Actions");
    schema.add_1d_analog(GameAnalog1D::BAR_HEIGHT, "Bar Height", "Demo");
    schema.add_2d_analog(GameAnalog2D::RETICLE_POS, "Reticle Position", "Demo");
    register_binds_schema(schema);
}

