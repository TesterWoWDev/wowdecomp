#pragma once

#include <map>
#include <string>
#include <vector>

namespace Patcher::Json
{
    enum class Type
    {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    struct Value
    {
        Type type    = Type::Null;
        bool boolean = false;
        std::string number;
        std::string string;
        std::vector<Value> array;
        std::map<std::string, Value> object;

        const Value* Find(const std::string& key) const;
    };

    bool Parse(const std::string& text, Value& out, std::string& error);
} // namespace Patcher::Json
