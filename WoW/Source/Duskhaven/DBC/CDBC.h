#pragma once

#include <cstdint>
#include <string>

class CDBC
{
public:
    CDBC() = default;
    virtual ~CDBC();

    uint32_t numColumns = 0;
    void* rawRows = nullptr;
    uint32_t rowSize = 0;
    uint32_t numRows = 0;
    void* stringTable = nullptr;
    uint32_t stringTableSize = 0;
    uint32_t minIndex = 0;
    uint32_t maxIndex = 0;

    bool LoadDB(const char* name);
    void UnloadDB();
    void ComputeIndexRange();

private:
    bool isLoaded = false;
};
