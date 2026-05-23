#include "engine/sexp_helpers.hpp"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace gubsy_sexp {

std::optional<std::string> read_text_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return std::nullopt;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

gsexp::Node find_root(const gsexp::ParseResult& parsed, std::string_view head) {
    for (std::size_t i = 0; i < parsed.root_count(); ++i) {
        gsexp::Node root = parsed.root(i);
        if (root.is_list() && root.head().is_atom(head))
            return root;
    }
    return {};
}

std::optional<int> field_to_int(gsexp::FormView form, std::string_view head) {
    return node_to_int(form.find_arg(head, 0));
}

std::optional<float> field_to_float(gsexp::FormView form, std::string_view head) {
    return node_to_float(form.find_arg(head, 0));
}

std::optional<std::string> field_to_string(gsexp::FormView form, std::string_view head) {
    return node_to_string(form.find_arg(head, 0));
}

std::optional<int> node_to_int(gsexp::Node node) {
    if (!node.valid() || node.type() != gsexp::ValueType::Atom)
        return std::nullopt;

    std::string_view text = node.text();
    if (gsexp::looks_like_integer(text)) {
        int value = 0;
        const char* begin = text.data();
        const char* end = text.data() + text.size();
        auto [ptr, ec] = std::from_chars(begin, end, value);
        if (ec == std::errc{} && ptr == end)
            return value;
    }

    if (!gsexp::looks_like_float(text))
        return std::nullopt;

    std::string owned(text);
    char* end_ptr = nullptr;
    float value = std::strtof(owned.c_str(), &end_ptr);
    if (end_ptr != owned.c_str() + owned.size())
        return std::nullopt;
    return static_cast<int>(value);
}

std::optional<float> node_to_float(gsexp::Node node) {
    if (!node.valid() || node.type() != gsexp::ValueType::Atom)
        return std::nullopt;

    std::string_view text = node.text();
    if (!gsexp::looks_like_integer(text) && !gsexp::looks_like_float(text))
        return std::nullopt;

    std::string owned(text);
    char* end_ptr = nullptr;
    float value = std::strtof(owned.c_str(), &end_ptr);
    if (end_ptr != owned.c_str() + owned.size())
        return std::nullopt;
    return value;
}

std::optional<std::string> node_to_string(gsexp::Node node) {
    if (!node.valid())
        return std::nullopt;
    if (node.type() != gsexp::ValueType::Atom && node.type() != gsexp::ValueType::String)
        return std::nullopt;
    return std::string(node.text());
}

std::optional<SettingsValue> node_to_settings_value(gsexp::Node node) {
    if (!node.valid())
        return std::nullopt;

    if (node.type() == gsexp::ValueType::String)
        return std::string(node.text());

    if (auto int_value = node_to_int(node)) {
        if (gsexp::looks_like_integer(node.text()))
            return *int_value;
    }

    if (auto float_value = node_to_float(node))
        return *float_value;

    if (!node.is_list() || !node.head().is_atom("vec2") || node.child_count() < 3)
        return std::nullopt;

    auto x = node_to_float(node.child_at(1));
    auto y = node_to_float(node.child_at(2));
    if (!x || !y)
        return std::nullopt;
    return SettingsVec2{*x, *y};
}

} // namespace gubsy_sexp
