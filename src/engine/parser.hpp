#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sexp {

enum class TokenType { LParen, RParen, Atom, String };

struct Token {
    TokenType type;
    std::string text;
};

struct SValue {
    enum class Type { List, Symbol, String, Int, Float };
    Type type{Type::List};
    std::string text;
    long long int_value{0};
    double float_value{0.0};
    std::vector<SValue> list;
};

bool looks_like_integer(const std::string& text);
bool looks_like_float(const std::string& text);

std::vector<Token> tokenize(const std::string& src);
bool parse_value(const std::vector<Token>& tokens, size_t& idx, SValue& out);
std::optional<std::vector<SValue>> parse_s_expressions(const std::string& text);

bool is_symbol(const SValue& value, const std::string& symbol);
const SValue* find_child(const SValue& list, const std::string& symbol);

std::optional<int> extract_int(const SValue& list, const std::string& symbol);
std::optional<float> extract_float(const SValue& list, const std::string& symbol);
std::optional<std::string> extract_string(const SValue& list, const std::string& symbol);

std::string quote_string(const std::string& text);

} // namespace sexp
