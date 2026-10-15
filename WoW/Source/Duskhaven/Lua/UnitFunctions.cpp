#include "WoW/Source/Duskhaven/Lua/UnitFunctions.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Unit.h"
#include "WoW/Source/Ui/ScriptEvents.h"

namespace Duskhaven
{
    namespace Lua
    {
        int Script_Duskhaven_GetUnitDisplayID(lua_State* L)
        {
            if (!ClientLua::IsString(L, 1))
                return 0;

            const char* name = ClientLua::ToString(L, 1);
            if (!name || !name[0])
                return 0;

            CGUnit_C* unit = GetUnitFromName(name);
            if (!unit || !unit->m_unit)
                return 0;

            uint32_t displayId = unit->m_unit->m_unitFieldDisplayid;
            if (displayId == 0 || displayId >= 0x80000000u)
                return 0;

            ClientLua::PushNumber(L, displayId);
            return 1;
        }

        void RegisterUnitFunctions()
        {
            ClientLua::AddFunction("Duskhaven_GetUnitDisplayID", Script_Duskhaven_GetUnitDisplayID);
        }
    } // namespace Lua
} // namespace Duskhaven
