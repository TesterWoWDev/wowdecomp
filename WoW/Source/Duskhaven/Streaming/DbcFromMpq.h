#pragma once

#include <string>
#include <vector>

namespace DbcFromMpq
{
    struct RefreshResult
    {
        bool spellDataChanged       = false;
        bool achievementDataChanged = false;
        bool interfaceFiles         = false;
        bool artFiles               = false;
    };

    RefreshResult RefreshFromArchive(void* hMpq, bool refreshNativeDbcs = true);
    // Member activation classifies art from these paths. Deletions can request a
    // conservative DBC/CDBC scan without broadening the art refresh.
    RefreshResult RefreshMembersFromArchive(void* hMpq, const std::vector<std::string>& memberPaths,
                                            bool refreshNativeDbcs = true, bool refreshAllDbcs = false);
} // namespace DbcFromMpq
