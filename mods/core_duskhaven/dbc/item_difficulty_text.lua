-- ==============================================================================
-- Duskhaven CDBC: ItemDifficultyText
-- ==============================================================================

local DBC = require("system.dbc")

local ItemDifficultyText = {}

local schema = { "itemID", "text", "red", "green", "blue" }

local defaultRows = {
    -- Example difficulty tags (Heroic, Mythic, Timewalking, Ascended)
    [100001] = { itemID = 100001, text = "Heroic", red = 0.0, green = 0.8, blue = 1.0 },
    [100002] = { itemID = 100002, text = "Mythic", red = 0.64, green = 0.21, blue = 0.93 },
    [100003] = { itemID = 100003, text = "Ascended", red = 1.0, green = 0.5, blue = 0.0 },
}

DBC.RegisterTable("ItemDifficultyText", schema, defaultRows)

function ItemDifficultyText.Register(itemId, text, red, green, blue)
    itemId = tonumber(itemId)
    if not itemId or not text then return end
    DBC.DefineRow("ItemDifficultyText", itemId, {
        itemID = itemId,
        text = tostring(text),
        red = tonumber(red) or 1.0,
        green = tonumber(green) or 1.0,
        blue = tonumber(blue) or 1.0
    })
end

function ItemDifficultyText.Get(itemId)
    return DBC.GetRow("ItemDifficultyText", itemId)
end

function ItemDifficultyText.Format(itemId)
    local row = DBC.GetRow("ItemDifficultyText", itemId)
    if not row or not row.text or row.text == "" then return nil end

    local r = math.floor((row.red or 1.0) * 255)
    local g = math.floor((row.green or 1.0) * 255)
    local b = math.floor((row.blue or 1.0) * 255)
    local hexColor = string.format("|cff%02x%02x%02x", r, g, b)
    return string.format("%s%s|r", hexColor, row.text)
end

_G.ItemDifficultyText = ItemDifficultyText
return ItemDifficultyText
