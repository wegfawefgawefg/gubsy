#include "ginput/io.hpp"

#include "gsexp/sexp.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

namespace ginput {
namespace {

void copy_parse_diagnostics(const gsexp::ParseResult& parsed, std::vector<Diagnostic>& out) {
    for (const gsexp::Diagnostic& diag : parsed.diagnostics) {
        out.push_back(Diagnostic{diag.message, diag.line, diag.column});
    }
}

gsexp::Node find_root(const gsexp::ParseResult& parsed, std::string_view root_name) {
    for (std::size_t index = 0; index < parsed.root_count(); ++index) {
        gsexp::Node root = parsed.root(index);
        if (root.is_list() && root.head().is_atom(root_name)) {
            return root;
        }
    }
    return {};
}

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return std::nullopt;
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

std::optional<int> first_int(gsexp::FormView& view, std::string_view primary,
                             std::string_view fallback) {
    if (std::optional<int> value = view.get_int(primary)) {
        return value;
    }
    return view.get_int(fallback);
}

bool write_file(const std::filesystem::path& path, std::string_view text) {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return false;
        }
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << text;
    return output.good();
}

void parse_button_binds(gsexp::Node node, InputProfile& profile) {
    if (!node.valid()) {
        return;
    }
    for (gsexp::Node child : node.children()) {
        if (!child.is_list() || !child.head().is_atom("bind")) {
            continue;
        }
        gsexp::FormView bind(child);
        std::optional<int> device_button = bind.get_int("device_button");
        std::optional<int> action = first_int(bind, "action", "gubsy_action");
        if (device_button && action) {
            add_button_bind(profile, ButtonBind{*device_button, *action});
        }
    }
}

void parse_axis_1d_binds(gsexp::Node node, InputProfile& profile) {
    if (!node.valid()) {
        return;
    }
    for (gsexp::Node child : node.children()) {
        if (!child.is_list() || !child.head().is_atom("bind")) {
            continue;
        }
        gsexp::FormView bind(child);
        std::optional<int> device_axis = bind.get_int("device_axis");
        std::optional<int> axis_1d = first_int(bind, "axis_1d", "gubsy_analog");
        if (!device_axis || !axis_1d) {
            continue;
        }
        Axis1DBind parsed{*device_axis, *axis_1d};
        if (std::optional<float> scale = bind.get_float("scale")) {
            parsed.scale = *scale;
        }
        if (std::optional<float> deadzone = bind.get_float("deadzone")) {
            parsed.deadzone = *deadzone;
        }
        add_axis_1d_bind(profile, parsed);
    }
}

void parse_axis_2d_binds(gsexp::Node node, InputProfile& profile) {
    if (!node.valid()) {
        return;
    }
    for (gsexp::Node child : node.children()) {
        if (!child.is_list() || !child.head().is_atom("bind")) {
            continue;
        }
        gsexp::FormView bind(child);
        std::optional<int> device_stick = bind.get_int("device_stick");
        std::optional<int> axis_2d = first_int(bind, "axis_2d", "gubsy_stick");
        if (!device_stick || !axis_2d) {
            continue;
        }
        Axis2DBind parsed{*device_stick, *axis_2d};
        if (std::optional<float> scale_x = bind.get_float("scale_x")) {
            parsed.scale_x = *scale_x;
        }
        if (std::optional<float> scale_y = bind.get_float("scale_y")) {
            parsed.scale_y = *scale_y;
        }
        if (std::optional<float> deadzone = bind.get_float("deadzone")) {
            parsed.deadzone = *deadzone;
        }
        add_axis_2d_bind(profile, parsed);
    }
}

void append_float_field(std::ostream& out, std::string_view key, float value, float default_value) {
    if (value == default_value) {
        return;
    }
    out << " (" << key << " " << value << ")";
}

InputProfile parse_profile(gsexp::Node node) {
    gsexp::FormView view(node);
    InputProfile profile;
    if (std::optional<int> id = view.get_int("id")) {
        profile.id = *id;
    }
    if (std::optional<std::string> name = view.get_string("name")) {
        profile.name = *name;
    }
    parse_button_binds(view.find("button_binds"), profile);
    parse_axis_1d_binds(view.find("analog_1d_binds"), profile);
    parse_axis_2d_binds(view.find("analog_2d_binds"), profile);
    return profile;
}

void append_profile(std::ostream& out, const InputProfile& profile) {
    out << "  (profile\n";
    out << "    (id " << profile.id << ")\n";
    out << "    (name " << gsexp::quote_string(profile.name) << ")\n";

    out << "    (button_binds\n";
    for (const ButtonBind& bind : profile.button_binds()) {
        out << "      (bind (device_button " << bind.device_button << ") (action " << bind.action
            << "))\n";
    }
    out << "    )\n";

    out << "    (analog_1d_binds\n";
    for (const Axis1DBind& bind : profile.axis_1d_binds()) {
        out << "      (bind (device_axis " << bind.device_axis << ") (axis_1d " << bind.axis_1d
            << ")";
        append_float_field(out, "scale", bind.scale, 1.0f);
        append_float_field(out, "deadzone", bind.deadzone, 0.0f);
        out << ")\n";
    }
    out << "    )\n";

    out << "    (analog_2d_binds\n";
    for (const Axis2DBind& bind : profile.axis_2d_binds()) {
        out << "      (bind (device_stick " << bind.device_stick << ") (axis_2d " << bind.axis_2d
            << ")";
        append_float_field(out, "scale_x", bind.scale_x, 1.0f);
        append_float_field(out, "scale_y", bind.scale_y, 1.0f);
        append_float_field(out, "deadzone", bind.deadzone, 0.0f);
        out << ")\n";
    }
    out << "    )\n";
    out << "  )\n";
}

} // namespace

LoadProfilesResult load_profiles_string(std::string_view text, std::string_view root_name) {
    LoadProfilesResult result;
    gsexp::ParseResult parsed = gsexp::parse(text);
    if (!parsed.ok) {
        copy_parse_diagnostics(parsed, result.diagnostics);
        return result;
    }

    gsexp::Node root = find_root(parsed, root_name);
    if (!root.valid()) {
        result.diagnostics.push_back(Diagnostic{"missing input profiles root form", 1, 1});
        return result;
    }

    for (gsexp::Node child : root.children()) {
        if (!child.is_list() || !child.head().is_atom("profile")) {
            continue;
        }
        InputProfile profile = parse_profile(child);
        if (profile.id <= 0 || profile.name.empty()) {
            result.diagnostics.push_back(
                Diagnostic{"skipped profile with missing id or name", 1, 1});
            continue;
        }
        result.profiles.push_back(std::move(profile));
    }

    result.ok = true;
    return result;
}

LoadProfilesResult load_profiles_string(std::string_view text, const Schema& schema,
                                        std::string_view root_name) {
    LoadProfilesResult result = load_profiles_string(text, root_name);
    if (result.ok) {
        result.reconcile_report = reconcile_profiles(result.profiles, schema);
    }
    return result;
}

LoadProfilesResult load_profiles_file(const std::filesystem::path& path,
                                      std::string_view root_name) {
    std::optional<std::string> text = read_file(path);
    if (!text) {
        return LoadProfilesResult{true, {}, {}, {}};
    }
    return load_profiles_string(*text, root_name);
}

LoadProfilesResult load_profiles_file(const std::filesystem::path& path, const Schema& schema,
                                      std::string_view root_name) {
    LoadProfilesResult result = load_profiles_file(path, root_name);
    if (result.ok) {
        result.reconcile_report = reconcile_profiles(result.profiles, schema);
    }
    return result;
}

bool save_profiles_string(const std::vector<InputProfile>& profiles, std::string& out,
                          std::string_view root_name) {
    std::ostringstream stream;
    stream << "(" << root_name << "\n";
    for (const InputProfile& profile : profiles) {
        append_profile(stream, profile);
    }
    stream << ")\n";
    out = stream.str();
    return true;
}

bool save_profiles_file(const std::filesystem::path& path,
                        const std::vector<InputProfile>& profiles, std::string_view root_name) {
    std::string text;
    if (!save_profiles_string(profiles, text, root_name)) {
        return false;
    }
    return write_file(path, text);
}

} // namespace ginput
