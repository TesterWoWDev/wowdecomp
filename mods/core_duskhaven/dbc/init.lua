-- ==============================================================================
-- Duskhaven CDBC Module Aggregator
-- ==============================================================================

local DBCModules = {
    SpellAdditionalAttributes = require("dbc.spell_additional_attributes"),
    ItemDifficultyText        = require("dbc.item_difficulty_text"),
    SpellAdditionalCostData   = require("dbc.spell_additional_cost_data"),
    LFGRoles                  = require("dbc.lfg_roles"),
    SpellEffects              = require("dbc.spell_effects"),
    VisualProfiles            = require("dbc.visual_profiles")
}

ModCore.LogInfo("Duskhaven CDBC tables loaded successfully.")
return DBCModules
