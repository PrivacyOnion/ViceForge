#pragma once

#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace viceforge::json {

struct Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

struct Value {
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
    Storage data;

    bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const noexcept { return std::holds_alternative<bool>(data); }
    bool is_number() const noexcept { return std::holds_alternative<double>(data); }
    bool is_string() const noexcept { return std::holds_alternative<std::string>(data); }
    bool is_array() const noexcept { return std::holds_alternative<Array>(data); }
    bool is_object() const noexcept { return std::holds_alternative<Object>(data); }

    const std::string& as_string() const { return std::get<std::string>(data); }
    bool as_bool() const { return std::get<bool>(data); }
    const Array& as_array() const { return std::get<Array>(data); }
    const Object& as_object() const { return std::get<Object>(data); }
};

class Parser {
public:
    explicit Parser(std::string_view input) : src_(input) {}

    bool parse(Value& out, std::string& error) {
        skip_ws();
        if (!parse_value(out, error)) {
            return false;
        }
        skip_ws();
        if (pos_ != src_.size()) {
            error = "Unexpected trailing characters in JSON";
            return false;
        }
        return true;
    }

private:
    std::string_view src_;
    size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
    }

    bool consume(char expected) {
        skip_ws();
        if (pos_ < src_.size() && src_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool parse_value(Value& out, std::string& error) {
        skip_ws();
        if (pos_ >= src_.size()) {
            error = "Unexpected end of JSON input";
            return false;
        }
        const char ch = src_[pos_];
        if (ch == '{') {
            return parse_object(out, error);
        }
        if (ch == '[') {
            return parse_array(out, error);
        }
        if (ch == '"') {
            std::string s;
            if (!parse_string(s, error)) {
                return false;
            }
            out.data = std::move(s);
            return true;
        }
        if (src_.substr(pos_).starts_with("true")) {
            pos_ += 4;
            out.data = true;
            return true;
        }
        if (src_.substr(pos_).starts_with("false")) {
            pos_ += 5;
            out.data = false;
            return true;
        }
        if (src_.substr(pos_).starts_with("null")) {
            pos_ += 4;
            out.data = nullptr;
            return true;
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            return parse_number(out, error);
        }
        error = std::string("Unexpected character '") + ch + "' in JSON";
        return false;
    }

    bool parse_string(std::string& out, std::string& error) {
        if (pos_ >= src_.size() || src_[pos_] != '"') {
            error = "Expected '\"' to start JSON string";
            return false;
        }
        ++pos_;
        out.clear();
        while (pos_ < src_.size()) {
            const char c = src_[pos_++];
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                error = "Unescaped control character in JSON string";
                return false;
            }
            if (c == '\\') {
                if (pos_ >= src_.size()) {
                    error = "Unterminated escape sequence in JSON string";
                    return false;
                }
                const char esc = src_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default:
                        error = std::string("Unsupported escape '\\") + esc + "' in JSON string";
                        return false;
                }
            } else {
                out.push_back(c);
            }
        }
        error = "Unterminated JSON string";
        return false;
    }

    bool parse_number(Value& out, std::string& error) {
        const size_t start = pos_;
        if (src_[pos_] == '-') {
            ++pos_;
        }
        if (pos_ >= src_.size() || !std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
            error = "Invalid JSON number";
            return false;
        }
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
        if (pos_ < src_.size() && src_[pos_] == '.') {
            ++pos_;
            if (pos_ >= src_.size() || !std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                error = "Invalid fraction in JSON number";
                return false;
            }
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                ++pos_;
            }
        }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) {
                ++pos_;
            }
            if (pos_ >= src_.size() || !std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                error = "Invalid exponent in JSON number";
                return false;
            }
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) {
                ++pos_;
            }
        }
        const std::string token(src_.substr(start, pos_ - start));
        try {
            out.data = std::stod(token);
        } catch (...) {
            error = "Number out of range in JSON";
            return false;
        }
        return true;
    }

    bool parse_array(Value& out, std::string& error) {
        ++pos_; // skip '['
        Array arr;
        skip_ws();
        if (consume(']')) {
            out.data = std::move(arr);
            return true;
        }
        while (true) {
            Value item;
            if (!parse_value(item, error)) {
                return false;
            }
            arr.push_back(std::move(item));
            skip_ws();
            if (consume(']')) {
                out.data = std::move(arr);
                return true;
            }
            if (!consume(',')) {
                error = "Expected ',' or ']' in JSON array";
                return false;
            }
        }
    }

    bool parse_object(Value& out, std::string& error) {
        ++pos_; // skip '{'
        Object obj;
        skip_ws();
        if (consume('}')) {
            out.data = std::move(obj);
            return true;
        }
        while (true) {
            skip_ws();
            std::string key;
            if (!parse_string(key, error)) {
                return false;
            }
            if (!consume(':')) {
                error = "Expected ':' after JSON object key";
                return false;
            }
            Value val;
            if (!parse_value(val, error)) {
                return false;
            }
            obj.insert_or_assign(std::move(key), std::move(val));
            skip_ws();
            if (consume('}')) {
                out.data = std::move(obj);
                return true;
            }
            if (!consume(',')) {
                error = "Expected ',' or '}' in JSON object";
                return false;
            }
        }
    }
};

} // namespace viceforge::json
