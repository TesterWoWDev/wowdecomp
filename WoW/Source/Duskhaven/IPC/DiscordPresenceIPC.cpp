#include "WoW/Source/Duskhaven/IPC/DiscordPresenceIPC.h"
#include "WoW/Source/Duskhaven/Lua/ClientLua.h"

namespace Duskhaven::IPC
{
    static int Script_DiscordSetGamePresence(lua_State* L)
    {
        // Stubbed out to prevent main thread stalls
        return 0;
    }

    static int Script_DiscordClearGamePresence(lua_State* L)
    {
        // Stubbed out to prevent main thread stalls
        return 0;
    }

    void RegisterLuaFunctions()
    {
        ClientLua::RegisterFunction("DiscordSetGamePresence", Script_DiscordSetGamePresence);
        ClientLua::RegisterFunction("DiscordClearGamePresence", Script_DiscordClearGamePresence);
    }
} // namespace Duskhaven::IPC
