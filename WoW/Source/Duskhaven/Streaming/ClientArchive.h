#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFile.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

namespace ClientArchive
{
    struct MountedArchive
    {
        void* publicHandle                      = nullptr;
        System_SFile::ARCHIVEREC* archiveRecord = nullptr;
        bool writable                           = false;
        System_SFile::ARCHIVEREC* originalArchiveRecord = nullptr;
    };

    bool FindMounted(const std::string& path, MountedArchive& out);
    bool EnsureMountedWritable(const std::string& path, MountedArchive& out, std::string& error);
    void PumpWritableRequests();

    inline bool Mount(const char* filename, int priority, SArchive** outHandle)
    {
        return SFile::OpenArchive(filename, priority, 0xC00, outHandle) != 0;
    }

    inline bool OpenRawFile(System_SFile::ARCHIVEREC* archiveRecord, const char* name, uint32_t flags, IOMpqUnit** outFile)
    {
        return archiveRecord && name && Blizzard::Mopaq::SFileOpenFileEx(archiveRecord, name, flags, outFile) != 0 && *outFile;
    }

    inline uint32_t GetRawFileSize(IOMpqUnit* file)
    {
        uint32_t high = 0;
        return file ? Blizzard::Mopaq::SFileGetFileSize(file, &high) : 0;
    }

    inline bool ReadRawFile(IOMpqUnit* file, void* buffer, uint32_t bytesToRead, uint32_t* bytesRead)
    {
        return file && Blizzard::Mopaq::SFileReadFile(file, buffer, bytesToRead, bytesRead);
    }

    inline bool ReleaseHandle(IOMpqUnit* file)
    {
        return file && Blizzard::Mopaq::SFileCloseFile(file);
    }

    inline bool CreateFile(System_SFile::ARCHIVEREC* archiveRecord, const char* name, uint32_t size, uint32_t flags, IOMpqUnit** outFile)
    {
        return archiveRecord && name && Blizzard::Mopaq::SFileCreateFile(archiveRecord, name, size, flags, outFile) && *outFile;
    }

    inline bool WriteCreatedFile(IOMpqUnit* file, const void* buffer, uint32_t bytes)
    {
        return file && Blizzard::Mopaq::SFileWriteFile(file, buffer, bytes);
    }

    inline bool WriteArchiveChanges(System_SFile::ARCHIVEREC* archiveRecord)
    {
        return archiveRecord && System_SFile::ARCHIVEREC::WriteArchiveChanges(archiveRecord);
    }

    inline bool ReadWholeFile(System_SFile::ARCHIVEREC* archiveRecord, const char* name, std::vector<uint8_t>& out)
    {
        IOMpqUnit* file = nullptr;
        if (!archiveRecord || !name || Blizzard::Mopaq::SFileOpenFileEx(archiveRecord, name, 0, &file) == 0 || !file)
            return false;

        uint32_t high = 0;
        const uint32_t size = Blizzard::Mopaq::SFileGetFileSize(file, &high);
        bool success        = false;
        if (size != 0 && size != 0xFFFFFFFFu)
        {
            out.resize(size);
            uint32_t bytesRead = 0;
            success            = Blizzard::Mopaq::SFileReadFile(file, out.data(), size, &bytesRead) && bytesRead == size;
            if (!success)
                out.clear();
        }

        Blizzard::Mopaq::SFileCloseFile(file);
        return success;
    }
} // namespace ClientArchive
