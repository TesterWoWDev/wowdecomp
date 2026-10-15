#pragma once

#include <cstddef>
#include <cstdint>

class SpellRec;

namespace Duskhaven
{
    namespace Tooltip
    {
        bool OwnedTooltipDescriptionHasValueMacro(const char* description);
        bool ParseOwnedSpellTooltipText(const SpellRec* spell, const char* source, char* dest, size_t destSize, uint32_t auraStackCount);
    } // namespace Tooltip
} // namespace Duskhaven
