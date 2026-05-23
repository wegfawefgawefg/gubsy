#include "engine/parser.hpp"

#include <algorithm>
#include <gsexp/sexp.hpp>

namespace sexp {
namespace {

TokenType convert_token_type(gsexp::TokenType type) {
    switch (type) {
    case gsexp::TokenType::LParen:
        return TokenType::LParen;
    case gsexp::TokenType::RParen:
        return TokenType::RParen;
    case gsexp::TokenType::String:
        return TokenType::String;
    case gsexp::TokenType::Atom:
        return TokenType::Atom;
    }
    return TokenType::Atom;
}

SValue atom_value(std::string text) {
    SValue atom;
    if (looks_like_integer(text)) {
        try {
            atom.type = SValue::Type::Int;
            atom.int_value = std::stoll(text);
            return atom;
        } catch (...) {
        }
    }
    if (looks_like_float(text)) {
        try {
            atom.type = SValue::Type::Float;
            atom.float_value = std::stod(text);
            return atom;
        } catch (...) {
        }
    }
    atom.type = SValue::Type::Symbol;
    atom.text = std::move(text);
    return atom;
}

SValue convert_node(gsexp::Node node) {
    if (node.is_list()) {
        SValue value;
        value.type = SValue::Type::List;
        for (gsexp::Node child : node.children())
            value.list.push_back(convert_node(child));
        return value;
    }
    if (node.is_string()) {
        SValue value;
        value.type = SValue::Type::String;
        value.text = std::string(node.text());
        return value;
    }
    return atom_value(std::string(node.text()));
}

} // namespace

bool looks_like_integer(const std::string& text) {
    return gsexp::looks_like_integer(text);
}

bool looks_like_float(const std::string& text) {
    return gsexp::looks_like_float(text);
}

std::vector<Token> tokenize(const std::string& src) {
    std::vector<gsexp::Diagnostic> diagnostics;
    std::vector<gsexp::Token> parsed = gsexp::tokenize(src, &diagnostics);
    std::vector<Token> tokens;
    tokens.reserve(parsed.size());
    for (const gsexp::Token& token : parsed)
        tokens.push_back(Token{convert_token_type(token.type), token.text});
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
        out = atom_value(tok.text);
        return true;
    }
    }
    return false;
}

std::optional<std::vector<SValue>> parse_s_expressions(const std::string& text) {
    gsexp::ParseResult parsed = gsexp::parse(text);
    if (!parsed.ok)
        return std::nullopt;

    std::vector<SValue> values;
    values.reserve(parsed.root_count());
    for (std::size_t i = 0; i < parsed.root_count(); ++i)
        values.push_back(convert_node(parsed.root(i)));
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
    return gsexp::quote_string(text);
}

} // namespace sexp
