#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class SpellRec;

namespace ModCore
{
    class TooltipEvaluator
    {
    public:
        // Returns true if text contains any macro tokens (@Value, @Period, @{...}, {{...}}, etc.)
        static bool HasMacro(const char* text);

        // Evaluates macros and Lua overrides in tooltip description text.
        // Returns true if evaluated and written to dest, false if no macros were found and not overridden.
        static bool EvaluateText(const SpellRec* spell, const char* source, char* dest, size_t destSize, uint32_t auraStackCount = 0);

        // Pre-expands special variable tokens like $<spellId>name and fixes unbraced variables ($hp -> ${hp})
        static std::string ExpandTooltipText(const SpellRec* spell, const char* text);
    };
}
