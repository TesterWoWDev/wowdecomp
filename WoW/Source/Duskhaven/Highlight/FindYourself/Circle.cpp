#include "WoW/Source/Duskhaven/Highlight/FindYourself/Circle.h"
#include "WoW/Source/Duskhaven/Highlight/FindYourself/FindYourself.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include <cstdlib>

namespace Duskhaven::Highlight::FindYourself::Circle
{
    namespace
    {
        bool s_enabled = false;

        bool FindYourselfCircleCVarCallback(CVar*, const char*, const char* value, void*)
        {
            s_enabled = value && std::atoi(value) != 0;
            FindYourself::RefreshPlayerNameColor();
            return true;
        }
    } // namespace

    void Apply()
    {
        CVar::Register("findYourselfModeCircle", "Draws a selection circle beneath the active player character", 1, "0",
                       FindYourselfCircleCVarCallback, CATEGORY_GRAPHICS, false, nullptr, false);
    }

    bool IsEnabled()
    {
        return s_enabled;
    }
}
