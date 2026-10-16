#pragma once

#include <string>

namespace StreamingBootstrap
{
    struct Status
    {
        bool exists = false;
        bool valid  = false;
        std::string manifestUrl;
        std::string detail;
    };

    Status Inspect(const char* filename);
} // namespace StreamingBootstrap
