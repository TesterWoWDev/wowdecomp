#include "WoW/Source/Duskhaven/Duskhaven.h"
#include "WoW/Source/Duskhaven/Config/DHConfig.h"
#include "WoW/Source/Duskhaven/CVars/DuskhavenCVars.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "WoW/Source/Duskhaven/Network/DuskhavenNetwork.h"
#include "WoW/Source/Duskhaven/Lua/PlayerFunctions.h"
#include "WoW/Source/Duskhaven/Lua/UnitFunctions.h"
#include "WoW/Source/Duskhaven/Lua/UIFunctions.h"
#include "WoW/Source/Duskhaven/Lua/ModelFunctions.h"
#include "WoW/Source/Duskhaven/Lua/SpellFunctions.h"
#include "WoW/Source/Duskhaven/Lua/HousingFunctions.h"
#include "WoW/Source/Duskhaven/Lua/CharCreateFunctions.h"
#include "WoW/Source/Duskhaven/Lua/EmbeddedScripts.h"

#include "WoW/Source/Duskhaven/Character/VisibleItemOverrides.h"
#include "WoW/Source/Duskhaven/Spells/ScriptedMissileVisuals.h"
#include "WoW/Source/Duskhaven/Display/GxMonitor.h"
#include "WoW/Source/Duskhaven/IPC/DiscordPresenceIPC.h"

namespace Duskhaven
{
    void Initialize()
    {
        // 1. Config
        DHConfig::Initialize();

        // 2. Custom CVars
        CVars::Initialize();

        // 3. Custom DBCs
        CDBCRegistry::LoadAll();

        // 4. Visible Item & Transmog Overrides
        VisibleItemOverrides::Initialize();

        // 5. Scripted Missile Visuals
        ScriptedMissileVisuals::Initialize();

        // 6. Multi-monitor support
        GxMonitor::Initialize();

        // 7. Network
        Network::Initialize();
    }

    void RegisterScriptFunctions()
    {
        DHConfig::RegisterLuaFunctions();
        Network::RegisterLuaFunctions();
        IPC::RegisterLuaFunctions();
        Lua::RegisterPlayerFunctions();
        Lua::RegisterUnitFunctions();
        Lua::RegisterUIFunctions();
        Lua::RegisterModelFunctions();
        Lua::RegisterSpellFunctions();
        Lua::RegisterHousingFunctions();
        Lua::RegisterCharCreateFunctions();
        Lua::RegisterEmbeddedScripts();
        GxMonitor::RegisterLuaFunctions();
    }
}
