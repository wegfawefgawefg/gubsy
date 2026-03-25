#include "engine/parser.hpp"

#include <algorithm>
#include <cctype>

namespace sexp {

bool looks_like_integer(const std::string& text) {
    if (text.empty())
        return false;
    size_t idx = 0;
    if (text[idx] == '+' || text[idx] == '-') {
        if (text.size() == 1)
            return false;
        ++idx;
    }
    bool digit = false;
    for (; idx < text.size(); ++idx) {
        if (!std::isdigit(static_cast<unsigned char>(text[idx])))
            return false;
        digit = true;
    }
    return digit;
}

bool looks_like_float(const std::string& text) {
    if (text.empty())
        return false;
    bool dot = false;
    bool exp = false;
    bool digit = false;
    size_t idx = 0;
    if (text[idx] == '+' || text[idx] == '-') {
        if (text.size() == 1)
            return false;
        ++idx;
    }
    for (; idx < text.size(); ++idx) {
        char c = text[idx];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            digit = true;
            continue;
        }
        if (c == '.' && !dot && !exp) {
            dot = true;
            continue;
        }
        if ((c == 'e' || c == 'E') && !exp && digit) {
            exp = true;
            digit = false;
            if (idx + 1 < text.size() && (text[idx + 1] == '+' || text[idx + 1] == '-')) {
                ++idx;
            }
            continue;
        }
        return false;
    }
    return digit && (dot || exp);
}

std::vector<Token> tokenize(const std::string& src) {
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < src.size()) {
        char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (c == ';' || c == '#') {
            while (i < src.size() && src[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (c == '(') {
            tokens.push_back({TokenType::LParen, "("});
            ++i;
            continue;
        }
        if (c == ')') {
            tokens.push_back({TokenType::RParen, ")"});
            ++i;
            continue;
        }
        if (c == '"') {
            ++i;
            std::string buffer;
            bool closed = false;
            while (i < src.size()) {
                char ch = src[i++];
                if (ch == '\\' && i < src.size()) {
                    char esc = src[i++];
                    switch (esc) {
                        case 'n': buffer.push_back('\n'); break;
                        case 'r': buffer.push_back('\r'); break;
                        case 't': buffer.push_back('\t'); break;
                        case '\\': buffer.push_back('\\'); break;
                        case '"': buffer.push_back('"'); break;
                        default: buffer.push_back(esc); break;
                    }
                } else if (ch == '"') {
                    closed = true;
                    break;
                } else {
                    buffer.push_back(ch);
                }
            }
            tokens.push_back({TokenType::String, buffer});
            if (!closed)
                break;
            continue;
        }
        size_t start = i;
        while (i < src.size()) {
            char ch = src[i];
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')')
                break;
            ++i;
        }
        tokens.push_back({TokenType::Atom, src.substr(start, i - start)});
    }
    return tokens;
}

bool parse_value(const std::vector<Token>& tokens, size_t& idx, SValue& out) {
    if (idx >= tokens.size())
        return false;
    const Token& tok = tokens[idx];
    switch (tok.type) {
        case TokenType::LParen: {
            ++idx;
            SValue list;
            list.type = SValue::Type::List;
            while (idx < tokens.size() && tokens[idx].type != TokenType::RParen) {
                SValue child;
                if (!parse_value(tokens, idx, child))
                    return false;
                list.list.push_back(std::move(child));
            }
            if (idx >= tokens.size() || tokens[idx].type != TokenType::RParen)
                return false;
            ++idx;
            out = std::move(list);
            return true;
        }
        case TokenType::RParen:
            return false;
        case TokenType::String: {
            ++idx;
            SValue val;
            val.type = SValue::Type::String;
            val.text = tok.text;
            out = std::move(val);
            return true;
        }
        case TokenType::Atom: {
            ++idx;
            SValue atom;
            if (looks_like_integer(tok.text)) {
                try {
                    atom.type = SValue::Type::Int;
                    atom.int_value = std::stoll(tok.text);
                } catch (...) {
                    atom.type = SValue::Type::Symbol;
                    atom.text = tok.text;
                }
            } else if (looks_like_float(tok.text)) {
                try {
                    atom.type = SValue::Type::Float;
                    atom.float_value = std::stod(tok.text);
                } catch (...) {
                    atom.type = SValue::Type::Symbol;
                    atom.text = tok.text;
                }
            } else {
                atom.type = SValue::Type::Symbol;
                atom.text = tok.text;
            }
            if (atom.type == SValue::Type::Symbol && atom.text.empty())
                atom.text = tok.text;
            out = std::move(atom);
            return true;
        }
    }
    return false;
}

std::optional<std::vector<SValue>> parse_s_expressions(const std::string& text) {
    auto tokens = tokenize(text);
    std::vector<SValue> values;
    size_t idx = 0;
    while (idx < tokens.size()) {
        SValue value;
        if (!parse_value(tokens, idx, value))
            return std::nullopt;
        values.push_back(std::move(value));
    }
    return values;
}

bool is_symbol(const SValue& value, const std::string& symbol) {
    return value.type == SValue::Type::Symbol && value.text == symbol;
}

const SValue* find_child(const SValue& list, const std::string& symbol) {
    if (list.type != SValue::Type::List)
        return nullptr;
    for (size_t i = 1; i < list.list.size(); ++i) {
        const SValue& child = list.list[i];
        if (child.type != SValue::Type::List || child.list.empty())
            continue;
        if (is_symbol(child.list.front(), symbol))
            return &child;
    }
    return nullptr;
}

std::optional<int> extract_int(const SValue& list, const std::string& symbol) {
    const SValue* node = find_child(list, symbol);
    if (!node || node->list.size() < 2)
        return std::nullopt;
    const SValue& val = node->list[1];
    if (val.type == SValue::Type::Int)
        return static_cast<int>(val.int_value);
    if (val.type == SValue::Type::Float)
        return static_cast<int>(val.float_value);
    if (val.type == SValue::Type::Symbol) {
        if (looks_like_integer(val.text)) {
            try {
                return std::stoi(val.text);
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<float> extract_float(const SValue& list, const std::string& symbol) {
    const SValue* node = find_child(list, symbol);
    if (!node || node->list.size() < 2)
        return std::nullopt;
    const SValue& val = node->list[1];
    if (val.type == SValue::Type::Float)
        return static_cast<float>(val.float_value);
    if (val.type == SValue::Type::Int)
        return static_cast<float>(val.int_value);
    if (val.type == SValue::Type::Symbol) {
        if (looks_like_float(val.text) || looks_like_integer(val.text)) {
            try {
                return std::stof(val.text);
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_string(const SValue& list, const std::string& symbol) {
    const SValue* node = find_child(list, symbol);
    if (!node || node->list.size() < 2)
        return std::nullopt;
    const SValue& val = node->list[1];
    if (val.type == SValue::Type::String || val.type == SValue::Type::Symbol)
        return val.text;
    return std::nullopt;
}

std::string quote_string(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    out.push_back('"');
    return out;
}

} // namespace sexp
