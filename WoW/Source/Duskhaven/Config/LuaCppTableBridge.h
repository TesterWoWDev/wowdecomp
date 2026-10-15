#pragma once

#include "Engine/Source/lua-5.1/src/lua.h"
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct LuaKey
{
    using Variant = std::variant<std::string, double, bool>;
    Variant data{std::string{}};

    LuaKey() = default;
    LuaKey(const std::string& v) : data(v) {}
    LuaKey(std::string&& v) : data(std::move(v)) {}
    LuaKey(double v) : data(v) {}
    LuaKey(bool v) : data(v) {}
};

struct LuaKeyLess
{
    bool operator()(const LuaKey& a, const LuaKey& b) const
    {
        if (a.data.index() != b.data.index())
            return a.data.index() < b.data.index();

        return std::visit(
            [](const auto& lhs, const auto& rhs) -> bool
            {
                using L = std::decay_t<decltype(lhs)>;
                using R = std::decay_t<decltype(rhs)>;
                if constexpr (std::is_same_v<L, R>)
                    return lhs < rhs;
                else
                    return false;
            },
            a.data, b.data);
    }
};

struct LuaValue
{
    using Object  = std::map<LuaKey, LuaValue, LuaKeyLess>;
    using Array   = std::vector<LuaValue>;
    using Variant = std::variant<std::nullptr_t, bool, double, std::string, Array, Object, std::uintptr_t>;

    Variant data{nullptr};

    LuaValue() = default;
    LuaValue(std::nullptr_t) : data(nullptr) {}
    LuaValue(bool v) : data(v) {}
    LuaValue(double v) : data(v) {}
    LuaValue(const std::string& v) : data(v) {}
    LuaValue(std::string&& v) : data(std::move(v)) {}
    LuaValue(const char* v) : data(std::string(v ? v : "")) {}
    LuaValue(const Array& v) : data(v) {}
    LuaValue(Array&& v) : data(std::move(v)) {}
    LuaValue(const Object& v) : data(v) {}
    LuaValue(Object&& v) : data(std::move(v)) {}
    LuaValue(std::uintptr_t v) : data(v) {}

    bool is_nil() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const { return std::holds_alternative<bool>(data); }
    bool is_number() const { return std::holds_alternative<double>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_array() const { return std::holds_alternative<Array>(data); }
    bool is_object() const { return std::holds_alternative<Object>(data); }

    const Object& as_object() const { return std::get<Object>(data); }
    Object& as_object() { return std::get<Object>(data); }

    const Array& as_array() const { return std::get<Array>(data); }
    Array& as_array() { return std::get<Array>(data); }

    const std::string& as_string() const { return std::get<std::string>(data); }
    double as_number() const { return std::get<double>(data); }
    bool as_bool() const { return std::get<bool>(data); }
};

namespace LuaCppBridge
{
    inline std::string FormatDouble(double value)
    {
        if (std::isnan(value)) return "0";
        if (std::isinf(value)) return (value < 0) ? "-math.huge" : "math.huge";

        std::ostringstream ss;
        ss << std::setprecision(15) << value;
        std::string s = ss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos)
        {
            s += ".0";
        }
        return s;
    }

    inline std::string EscapeString(const std::string& str)
    {
        std::string out;
        out.reserve(str.size() + 2);
        out.push_back('"');
        for (char ch : str)
        {
            switch (ch)
            {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20)
                    {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\%03u", static_cast<unsigned char>(ch));
                        out += buf;
                    }
                    else
                    {
                        out.push_back(ch);
                    }
                    break;
            }
        }
        out.push_back('"');
        return out;
    }

    inline bool IsValidIdentifier(const std::string& s)
    {
        if (s.empty()) return false;
        char first = s[0];
        if (!std::isalpha(static_cast<unsigned char>(first)) && first != '_') return false;

        for (size_t i = 1; i < s.size(); ++i)
        {
            char ch = s[i];
            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') return false;
        }

        static const std::set<std::string_view> kReserved = {
            "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
            "if", "in", "local", "nil", "not", "or", "repeat", "return", "then",
            "true", "until", "while"
        };
        return kReserved.find(s) == kReserved.end();
    }

    inline void SerializeValue(const LuaValue& v, std::string& out, int indent, bool pretty);

    inline void SerializeTable(const LuaValue::Object& obj, std::string& out, int indent, bool pretty)
    {
        if (obj.empty())
        {
            out += "{}";
            return;
        }

        out += "{";
        if (pretty) out += "\n";

        bool first = true;
        for (const auto& [k, v] : obj)
        {
            if (!first)
            {
                out += ",";
                if (pretty) out += "\n";
            }
            first = false;

            if (pretty) out.append(static_cast<size_t>(indent) * 2, ' ');

            std::visit(
                [&](const auto& keyVal)
                {
                    using T = std::decay_t<decltype(keyVal)>;
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        if (IsValidIdentifier(keyVal))
                        {
                            out += keyVal;
                            out += " = ";
                        }
                        else
                        {
                            out += "[";
                            out += EscapeString(keyVal);
                            out += "] = ";
                        }
                    }
                    else if constexpr (std::is_same_v<T, double>)
                    {
                        out += "[";
                        out += FormatDouble(keyVal);
                        out += "] = ";
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        out += "[";
                        out += (keyVal ? "true" : "false");
                        out += "] = ";
                    }
                },
                k.data);

            SerializeValue(v, out, indent + 1, pretty);
        }

        if (pretty)
        {
            out += "\n";
            out.append(static_cast<size_t>(indent - 1) * 2, ' ');
        }
        out += "}";
    }

    inline void SerializeArray(const LuaValue::Array& arr, std::string& out, int indent, bool pretty)
    {
        if (arr.empty())
        {
            out += "{}";
            return;
        }

        out += "{";
        if (pretty) out += "\n";

        for (size_t i = 0; i < arr.size(); ++i)
        {
            if (i > 0)
            {
                out += ",";
                if (pretty) out += "\n";
            }
            if (pretty) out.append(static_cast<size_t>(indent) * 2, ' ');
            SerializeValue(arr[i], out, indent + 1, pretty);
        }

        if (pretty)
        {
            out += "\n";
            out.append(static_cast<size_t>(indent - 1) * 2, ' ');
        }
        out += "}";
    }

    inline void SerializeValue(const LuaValue& v, std::string& out, int indent, bool pretty)
    {
        std::visit(
            [&](const auto& val)
            {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>)
                {
                    out += "nil";
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    out += (val ? "true" : "false");
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    out += FormatDouble(val);
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    out += EscapeString(val);
                }
                else if constexpr (std::is_same_v<T, LuaValue::Array>)
                {
                    SerializeArray(val, out, indent, pretty);
                }
                else if constexpr (std::is_same_v<T, LuaValue::Object>)
                {
                    SerializeTable(val, out, indent, pretty);
                }
                else if constexpr (std::is_same_v<T, std::uintptr_t>)
                {
                    out += "nil";
                }
            },
            v.data);
    }

    inline std::string ToLuaSource(const LuaValue& val, bool pretty = false)
    {
        std::string out;
        SerializeValue(val, out, 1, pretty);
        return out;
    }

    inline LuaValue ReadValue(lua_State* L, int index);

    inline LuaValue ReadTable(lua_State* L, int index)
    {
        if (index < 0) index = lua_gettop(L) + index + 1;

        LuaValue::Object obj;
        lua_pushnil(L);
        while (lua_next(L, index) != 0)
        {
            LuaKey key;
            int keyType = lua_type(L, -2);
            if (keyType == LUA_TSTRING)
            {
                key = LuaKey(std::string(lua_tostring(L, -2)));
            }
            else if (keyType == LUA_TNUMBER)
            {
                key = LuaKey(lua_tonumber(L, -2));
            }
            else if (keyType == LUA_TBOOLEAN)
            {
                key = LuaKey(lua_toboolean(L, -2) != 0);
            }
            else
            {
                lua_pop(L, 1);
                continue;
            }

            obj.emplace(std::move(key), ReadValue(L, -1));
            lua_pop(L, 1);
        }
        return LuaValue(std::move(obj));
    }

    inline LuaValue ReadValue(lua_State* L, int index)
    {
        if (index < 0) index = lua_gettop(L) + index + 1;
        int t = lua_type(L, index);
        switch (t)
        {
            case LUA_TNIL:     return LuaValue(nullptr);
            case LUA_TBOOLEAN: return LuaValue(lua_toboolean(L, index) != 0);
            case LUA_TNUMBER:  return LuaValue(lua_tonumber(L, index));
            case LUA_TSTRING:  return LuaValue(std::string(lua_tostring(L, index)));
            case LUA_TTABLE:   return ReadTable(L, index);
            default:           return LuaValue(nullptr);
        }
    }
} // namespace LuaCppBridge
