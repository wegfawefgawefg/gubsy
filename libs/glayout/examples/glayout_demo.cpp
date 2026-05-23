#include "glayout/layout.hpp"

#include <iostream>
#include <vector>

int main() {
    std::vector<glayout::Layout> layouts{
        glayout::Layout{100, "Title", 1280, 720, glayout::FormFactor::Desktop, {}},
        glayout::Layout{100, "Title", 1920, 1080, glayout::FormFactor::Desktop, {}},
        glayout::Layout{100, "Title", 1080, 1920, glayout::FormFactor::Phone, {}},
    };

    glayout::Object play_button{};
    play_button.id = 1;
    play_button.label = "play_button";
    play_button.rect = glayout::Rect{0.4f, 0.45f, 0.2f, 0.08f};
    glayout::add_or_replace_object(layouts[1], play_button);

    const glayout::Layout* layout =
        glayout::find_best_layout(layouts, 100, 1920, 1080, glayout::FormFactor::Desktop);
    if (!layout) {
        std::cerr << "No layout found\n";
        return 1;
    }

    std::cout << "Selected layout: " << layout->label << " " << layout->width << "x"
              << layout->height << "\n";

    const glayout::Object* object = glayout::find_object(*layout, "play_button");
    if (object) {
        std::cout << "Found object: " << object->label << " at " << object->rect.x << ", "
                  << object->rect.y << "\n";
    }

    std::string text = glayout::write_layouts(layouts);
    glayout::ParseResult parsed = glayout::parse_layouts(text);
    std::cout << "Round-tripped layouts: " << parsed.layouts.size() << "\n";

    return 0;
}
