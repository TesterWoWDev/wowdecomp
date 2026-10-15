#include "WoW/Source/Duskhaven/DBC/CDBC.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstdio>
#include <cstring>

CDBC::~CDBC()
{
    UnloadDB();
}

bool CDBC::LoadDB(const char* name)
{
    if (this->isLoaded)
        return true;

    char filePath[512];
    std::snprintf(filePath, sizeof(filePath), "DBFilesClient\\%s.cdbc", name);

    SFileImpl* fileBlock = nullptr;
    if (!SFile::OpenEx(nullptr, filePath, 0x20000, &fileBlock))
    {
        // Try fallback with .dbc extension if .cdbc is not found
        std::snprintf(filePath, sizeof(filePath), "DBFilesClient\\%s.dbc", name);
        if (!SFile::OpenEx(nullptr, filePath, 0x20000, &fileBlock))
            return false;
    }

    uint32_t signature = 0;
    if (!SFileReadFile(fileBlock, &signature, sizeof(signature), nullptr, nullptr, 0))
    {
        SFileCloseFile(fileBlock);
        return false;
    }

    if (signature != 0x43424457) // 'WDBC'
    {
        SFileCloseFile(fileBlock);
        return false;
    }

    if (!SFileReadFile(fileBlock, &this->numRows, sizeof(this->numRows), nullptr, nullptr, 0))
    {
        SFileCloseFile(fileBlock);
        return false;
    }

    if (!SFileReadFile(fileBlock, &this->numColumns, sizeof(this->numColumns), nullptr, nullptr, 0))
    {
        SFileCloseFile(fileBlock);
        return false;
    }

    if (!SFileReadFile(fileBlock, &this->rowSize, sizeof(this->rowSize), nullptr, nullptr, 0))
    {
        SFileCloseFile(fileBlock);
        return false;
    }

    if (!SFileReadFile(fileBlock, &this->stringTableSize, sizeof(this->stringTableSize), nullptr, nullptr, 0))
    {
        SFileCloseFile(fileBlock);
        return false;
    }

    if (this->numRows > 0 && this->rowSize > 0)
    {
        uint32_t totalRowBytes = this->numRows * this->rowSize;
        this->rawRows = SMemAlloc(totalRowBytes, __FILE__, __LINE__, 0);
        if (!this->rawRows || !SFileReadFile(fileBlock, this->rawRows, totalRowBytes, nullptr, nullptr, 0))
        {
            UnloadDB();
            SFileCloseFile(fileBlock);
            return false;
        }
    }

    if (this->stringTableSize > 0)
    {
        this->stringTable = SMemAlloc(this->stringTableSize, __FILE__, __LINE__, 0);
        if (!this->stringTable || !SFileReadFile(fileBlock, this->stringTable, this->stringTableSize, nullptr, nullptr, 0))
        {
            UnloadDB();
            SFileCloseFile(fileBlock);
            return false;
        }
    }

    ComputeIndexRange();
    this->isLoaded = true;
    SFileCloseFile(fileBlock);
    return true;
}

void CDBC::UnloadDB()
{
    if (this->rawRows)
    {
        SMemFree(this->rawRows, __FILE__, __LINE__, 0);
        this->rawRows = nullptr;
    }

    if (this->stringTable)
    {
        SMemFree(this->stringTable, __FILE__, __LINE__, 0);
        this->stringTable = nullptr;
    }

    this->numRows = 0;
    this->numColumns = 0;
    this->rowSize = 0;
    this->stringTableSize = 0;
    this->minIndex = 0;
    this->maxIndex = 0;
    this->isLoaded = false;
}

void CDBC::ComputeIndexRange()
{
    if (this->numRows == 0 || !this->rawRows || this->numColumns == 0)
        return;

    const uint32_t* firstRow = static_cast<const uint32_t*>(this->rawRows);
    const uint32_t* lastRow = firstRow + ((this->numRows - 1) * this->numColumns);
    this->minIndex = *firstRow;
    this->maxIndex = *lastRow;
}
