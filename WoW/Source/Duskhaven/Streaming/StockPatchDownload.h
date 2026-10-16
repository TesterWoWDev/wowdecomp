#pragma once

namespace ContentStreaming
{
    inline constexpr const char* kStockPatchDownloadLua = R"LUA(
local active = false
local pollElapsed = 0
local speedElapsed = 0
local previousBytes = 0
local bytesPerSecond = 0
local previousPhase = ""
local stockEventCaptured = false
local stockOnEvent = nil
local stockOnUpdate = nil
local OnUpdate

local function IgnoreStockPatchEvent()
end

local function ClaimPatchDownloadFrame()
    if not stockEventCaptured then
        stockOnEvent = PatchDownload:GetScript("OnEvent")
        stockOnUpdate = PatchDownload:GetScript("OnUpdate")
        stockEventCaptured = true
    end
    PatchDownload:SetScript("OnEvent", IgnoreStockPatchEvent)
end

local function ReleasePatchDownloadFrame()
    if stockEventCaptured then
        if PatchDownload:GetScript("OnEvent") == IgnoreStockPatchEvent then
            PatchDownload:SetScript("OnEvent", stockOnEvent)
        end
        if PatchDownload:GetScript("OnUpdate") == OnUpdate then
            PatchDownload:SetScript("OnUpdate", stockOnUpdate)
        end
        stockOnEvent = nil
        stockOnUpdate = nil
        stockEventCaptured = false
    end
end

local function FormatBytes(bytes)
    if bytes >= 1073741824 then
        return format("%.2f GB", bytes / 1073741824)
    elseif bytes >= 1048576 then
        return format("%.1f MB", bytes / 1048576)
    elseif bytes >= 1024 then
        return format("%.1f KB", bytes / 1024)
    end
    return format("%d B", bytes)
end

local function GetPatchLabel(path)
    if string.find(path or "", "Client runtime :: ", 1, true) then
        return "Client update"
    end
    local archive = string.match(path or "", "^(.-) :: ") or path or ""
    local filename = string.match(archive, "[^/\\]+$") or ""
    local patch = string.match(filename, "^[Pp][Aa][Tt][Cc][Hh]%-([%w%-]+)%.[Mm][Pp][Qq]$")
    return patch and "Patch " .. string.upper(patch) or filename
end

local function GetState()
    if type(Duskhaven_GetContentStreamingProgress) ~= "function" then
        return nil
    end

    local progress, busy, complete, _, filesDone, filesTotal, bytesDone, bytesTotal, currentFile, phase, errorText =
        Duskhaven_GetContentStreamingProgress()
    return {
        progress = progress or 0,
        busy = busy == true,
        complete = complete == true,
        filesDone = filesDone or 0,
        filesTotal = filesTotal or 0,
        bytesDone = bytesDone or 0,
        bytesTotal = bytesTotal or 0,
        currentFile = currentFile or "",
        phase = phase or "unknown",
        errorText = errorText or "",
    }
end

local function GetTitle(phase, currentFile)
    if phase == "checking" then
        return "Checking game files"
    elseif phase == "downloading" then
        if string.find(currentFile, "Client runtime :: ", 1, true) then
            return "Downloading client update"
        end
        return "Downloading patches"
    elseif phase == "installing" then
        return "Applying game files"
    elseif phase == "verifying" then
        return "Verifying game files"
    end
    return "Updating game files"
end

local function GetStageLine(phase)
    local function Stage(label, stagePhase)
        return phase == stagePhase and "|cffffd200" .. label .. "|r" or "|cff777777" .. label .. "|r"
    end
    return Stage("CHECK", "checking") .. "     " .. Stage("DOWNLOAD", "downloading") .. "     " ..
        Stage("APPLY", "installing") .. "     " .. Stage("VERIFY", "verifying")
end

local function ShowActiveLayout()
    DownloadingUpdateTitle:Show()
    PatchProgressText:Show()
    PatchSuccessfulTitle:Hide()
    PatchSuccessfulText:Hide()
    PatchDownloadCancelButton:Hide()
    PatchDownloadRestartButton:Hide()
end

local function UpdateDisplay()
    local state = GetState()
    if not state then
        return
    end

    if state.phase == "failed" or state.phase == "canceled" then
        DownloadingUpdateTitle:SetText(state.phase == "canceled" and "Update canceled" or "Update Failed")
        DownloadingUpdateTitle:Show()
        PatchProgressText:SetText(state.errorText ~= "" and state.errorText or
            (state.phase == "canceled" and "Restart the client to resume downloading the required game files." or
             "The game update could not be downloaded."))
        PatchProgressText:Show()
        PatchSuccessfulTitle:Hide()
        PatchSuccessfulText:Hide()
        PatchDownloadCancelButton:Hide()
        PatchDownloadRestartButton:SetText("Exit")
        PatchDownloadRestartButton:SetScript("OnClick", QuitGame)
        PatchDownloadRestartButton:Show()
        return
    end

    if state.phase == "restart-required" then
        active = true
        PatchDownload:SetScript("OnUpdate", nil)
        ShowActiveLayout()
        DownloadingUpdateTitle:SetText("Client Restart Required")
        PatchProgressText:SetText(state.errorText ~= "" and state.errorText or
            "Specific game files have been downloaded that require a client restart to take effect.")
        PatchDownloadRestartButton:SetText("Exit")
        PatchDownloadRestartButton:SetScript("OnClick", QuitGame)
        PatchDownloadRestartButton:Show()
        return
    end

    if state.complete or state.phase == "disabled" then
        active = false
        ReleasePatchDownloadFrame()
        SetGlueScreen("login")
        return
    end

    ShowActiveLayout()
    DownloadingUpdateTitle:SetText(GetTitle(state.phase, state.currentFile))

    local percentage = math.floor(state.progress * 100 + 0.5)
    local details
    local patch = GetPatchLabel(state.currentFile)
    if state.phase == "downloading" then
        local speed = bytesPerSecond > 0 and FormatBytes(bytesPerSecond) .. "/s" or ""
        local speedSeparator = patch ~= "" and speed ~= "" and "  -  " or ""
        details = format("%d%%  -  %s / %s\n%s%s%s", percentage,
            FormatBytes(state.bytesDone), FormatBytes(state.bytesTotal), patch, speedSeparator, speed)
    elseif state.phase == "checking" and state.filesTotal == 0 then
        details = "Checking for updates..."
    else
        details = format("%d%%%s", percentage, patch ~= "" and "\n" .. patch or "")
    end

    PatchProgressText:SetText(GetStageLine(state.phase) .. "\n\n" .. details)
end

OnUpdate = function(_, elapsed)
    pollElapsed = pollElapsed + elapsed
    speedElapsed = speedElapsed + elapsed
    if pollElapsed < 0.1 then
        return
    end
    pollElapsed = 0

    local state = GetState()
    if state then
        if state.phase ~= previousPhase then
            previousPhase = state.phase
            previousBytes = state.bytesDone
            bytesPerSecond = 0
            speedElapsed = 0
        elseif state.phase == "downloading" and speedElapsed >= 1 then
            bytesPerSecond = math.max(0, state.bytesDone - previousBytes) / speedElapsed
            previousBytes = state.bytesDone
            speedElapsed = 0
        elseif state.phase ~= "downloading" then
            bytesPerSecond = 0
        end
    end
    UpdateDisplay()
end

function Duskhaven_ShowStockContentStreamingScreen(preserveTerminalState)
    if DuskhavenContentStreamingScreenActive ~= nil then
        if active or stockEventCaptured then
            active = false
            ReleasePatchDownloadFrame()
        end
        local state = GetState()
        if state and (state.busy or state.phase == "failed" or state.phase == "canceled" or (preserveTerminalState and state.complete)) then
            DuskhavenContentStreamingScreenActive = true
            DuskhavenContentStreamingReturnScreen = "login"
            if type(GetCurrentGlueScreenName) ~= "function" or GetCurrentGlueScreenName() ~= "patchdownload" then
                SetGlueScreen("patchdownload")
            end
            return true
        end
        return false
    end
    if active then
        if type(GetCurrentGlueScreenName) ~= "function" or GetCurrentGlueScreenName() ~= "patchdownload" then
            SetGlueScreen("patchdownload")
        end
        ClaimPatchDownloadFrame()
        PatchDownload:SetScript("OnUpdate", OnUpdate)
        UpdateDisplay()
        return true
    end
    if not PatchDownloadUI or not PatchDownload or not PatchProgressText or not PatchDownloadLogo or not DownloadingUpdateTitle or
        not PatchSuccessfulTitle or not PatchSuccessfulText or not PatchDownloadCancelButton or
        not PatchDownloadRestartButton or type(SetGlueScreen) ~= "function" or type(QuitGame) ~= "function" then
        return false
    end

    local state = GetState()
    if not state or (not state.busy and state.phase ~= "failed" and state.phase ~= "canceled") then
        return false
    end

    active = true
    pollElapsed = 0
    speedElapsed = 0
    previousBytes = state.bytesDone
    bytesPerSecond = 0
    previousPhase = state.phase

    ClaimPatchDownloadFrame()
    SetGlueScreen("patchdownload")
    PatchDownloadLogo:SetTexture("Interface\\Glues\\Common\\Glues-WoW-WotLKLogo")
    DownloadingUpdateTitle:ClearAllPoints()
    DownloadingUpdateTitle:SetPoint("CENTER", PatchDownloadUI, "CENTER", 0, 100)
    PatchDownloadLogo:ClearAllPoints()
    PatchDownloadLogo:SetPoint("BOTTOM", "DownloadingUpdateTitle", "TOP", 0, 20)
    PatchProgressText:ClearAllPoints()
    PatchProgressText:SetPoint("TOP", "DownloadingUpdateTitle", "BOTTOM", 0, -16)
    PatchProgressText:SetWidth(900)
    PatchProgressText:SetHeight(120)
    PatchProgressText:SetJustifyH("CENTER")
    PatchProgressText:SetJustifyV("TOP")
    PatchDownloadCancelButton:ClearAllPoints()
    PatchDownloadCancelButton:SetPoint("CENTER", PatchDownloadUI, "CENTER", 0, -120)
    PatchDownloadRestartButton:ClearAllPoints()
    PatchDownloadRestartButton:SetPoint("CENTER", PatchDownloadUI, "CENTER", 0, -120)
    PatchDownload:SetScript("OnUpdate", OnUpdate)
    UpdateDisplay()
    return true
end
)LUA";
} // namespace ContentStreaming
