#include "DbcFromMpq.h"
#include "WoW/Source/Duskhaven/DBC/CDBCRegistry.h"
#include "StreamingLogger.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "ClientArchive.h"
#include "NativeDbcPatch.h"

namespace DbcFromMpq
{
    namespace
    {
        struct DbcCopy
        {
            uint16_t structOff;
            uint16_t fileOff;
            uint16_t bytes;
        };

        struct DbcStr
        {
            uint16_t structOff;
            uint16_t fileOff;
        };

        struct DbcTransform
        {
            const char* name;
            uint16_t structSize;
            uint16_t fileRecordSize;
            const DbcCopy* copies;
            uint8_t copyCount;
            const DbcStr* strings;
            uint8_t strCount;
            uint16_t idOff;
            uint8_t idGenerated;
        };

        struct RefreshState
        {
            bool characterComponentChanged = false;
            bool groundEffectDoodadChanged = false;
        };

#include "DbcTransforms.inc"

#pragma pack(push, 1)
        struct WdbcHeader
        {
            char magic[4];
            uint32_t recordCount;
            uint32_t fieldCount;
            uint32_t recordSize;
            uint32_t stringSize;
        };
#pragma pack(pop)

        bool EqualsIgnoreCase(const char* left, const char* right, size_t length)
        {
            for (size_t index = 0; index < length; ++index)
            {
                if (std::tolower(static_cast<unsigned char>(left[index])) !=
                    std::tolower(static_cast<unsigned char>(right[index])))
                    return false;
            }
            return true;
        }

        bool NameEquals(const std::string& left, const char* right)
        {
            const size_t length = std::strlen(right);
            return left.size() == length && EqualsIgnoreCase(left.c_str(), right, length);
        }

        bool ContainsIgnoreCase(const char* text, size_t textLength, const char* value, size_t valueLength)
        {
            if (valueLength == 0 || textLength < valueLength)
                return false;

            for (size_t index = 0; index + valueLength <= textLength; ++index)
            {
                if (EqualsIgnoreCase(text + index, value, valueLength))
                    return true;
            }
            return false;
        }

        const DbcTransform* FindTransform(const char* stem, size_t stemLength)
        {
            for (const DbcTransform& transform : kTransforms)
            {
                if (std::strlen(transform.name) == stemLength && EqualsIgnoreCase(transform.name, stem, stemLength))
                    return &transform;
            }
            return nullptr;
        }

        bool ReadHeader(const std::vector<uint8_t>& data, WdbcHeader& header, size_t& recordBytes)
        {
            if (data.size() < sizeof(WdbcHeader))
                return false;

            std::memcpy(&header, data.data(), sizeof(header));
            if (std::memcmp(header.magic, "WDBC", 4) != 0 || header.recordSize == 0 || header.recordCount == 0)
                return false;

            if (header.recordCount > (std::numeric_limits<size_t>::max)() / header.recordSize)
                return false;
            recordBytes = static_cast<size_t>(header.recordCount) * header.recordSize;
            return recordBytes <= data.size() - sizeof(WdbcHeader) &&
                   header.stringSize <= data.size() - sizeof(WdbcHeader) - recordBytes;
        }

        void ApplyNativeDbc(void* archive, const std::string& path, const char* stem, size_t stemLength,
                            bool& characterComponentChanged, bool& groundEffectDoodadChanged)
        {
            const DbcTransform* transform = FindTransform(stem, stemLength);
            const std::string name(stem, stemLength);
            if (!transform)
            {
                LOG_DEBUG << "ContentStreaming: native DBC has no verified transform name=" << name;
                return;
            }

            std::vector<uint8_t> data;
            if (!ClientArchive::ReadWholeFile(static_cast<System_SFile::ARCHIVEREC*>(archive), path.c_str(), data))
            {
                LOG_WARN << "ContentStreaming: could not read streamed DBC path=" << path;
                return;
            }

            WdbcHeader header{};
            size_t recordBytes = 0;
            if (!ReadHeader(data, header, recordBytes))
            {
                LOG_WARN << "ContentStreaming: invalid streamed DBC path=" << path;
                return;
            }
            if (header.recordSize != transform->fileRecordSize)
            {
                LOG_WARN << "ContentStreaming: streamed DBC row size mismatch name=" << name
                         << " found=" << header.recordSize << " expected=" << transform->fileRecordSize;
                return;
            }

            const uint8_t* records = data.data() + sizeof(WdbcHeader);
            const char* strings    = reinterpret_cast<const char*>(records + recordBytes);
            if (transform->structSize == 0 ||
                header.recordCount > (std::numeric_limits<size_t>::max)() / transform->structSize)
            {
                LOG_WARN << "ContentStreaming: transformed DBC size overflow name=" << name;
                return;
            }
            const size_t imageBytes = static_cast<size_t>(header.recordCount) * transform->structSize;
            std::vector<uint8_t> images(imageBytes, 0);
            std::vector<uint32_t> ids(header.recordCount);

            for (uint32_t row = 0; row < header.recordCount; ++row)
            {
                const uint8_t* source = records + static_cast<size_t>(row) * header.recordSize;
                uint8_t* destination  = images.data() + static_cast<size_t>(row) * transform->structSize;
                for (uint8_t copy = 0; copy < transform->copyCount; ++copy)
                {
                    const DbcCopy& field = transform->copies[copy];
                    std::memcpy(destination + field.structOff, source + field.fileOff, field.bytes);
                }
                for (uint8_t stringIndex = 0; stringIndex < transform->strCount; ++stringIndex)
                {
                    const DbcStr& field = transform->strings[stringIndex];
                    std::memcpy(destination + field.structOff, source + field.fileOff, sizeof(uint32_t));
                }
                if (transform->idGenerated)
                    std::memcpy(destination + transform->idOff, &row, sizeof(uint32_t));
                std::memcpy(&ids[row], destination + transform->idOff, sizeof(uint32_t));
            }

            std::vector<uint16_t> stringOffsets(transform->strCount);
            for (uint8_t index = 0; index < transform->strCount; ++index)
                stringOffsets[index] = transform->strings[index].structOff;

            if (DBCPatch::ApplyRecords(name.c_str(), transform->structSize, stringOffsets, ids, images.data(), strings,
                                       header.stringSize, transform->idGenerated != 0))
            {
                characterComponentChanged = characterComponentChanged || NameEquals(name, "CharSections") ||
                                            NameEquals(name, "CharacterFacialHairStyles") ||
                                            NameEquals(name, "ChrRaces");
                groundEffectDoodadChanged = groundEffectDoodadChanged || NameEquals(name, "GroundEffectDoodad");
                LOG_INFO << "ContentStreaming: refreshed native DBC name=" << name << " rows=" << header.recordCount;
            }
        }

        void ProcessDbcEntry(void* archive, const char* entry, size_t length, RefreshResult& result,
                             bool refreshNativeDbcs, RefreshState& state)
        {
            static constexpr char PREFIX[]         = "dbfilesclient\\";
            static constexpr char DBC_EXTENSION[]  = ".dbc";
            static constexpr char CDBC_EXTENSION[] = ".cdbc";
            constexpr size_t prefixLength          = sizeof(PREFIX) - 1;
            constexpr size_t dbcExtensionLength    = sizeof(DBC_EXTENSION) - 1;
            constexpr size_t cdbcExtensionLength   = sizeof(CDBC_EXTENSION) - 1;

            if (length <= prefixLength || !EqualsIgnoreCase(entry, PREFIX, prefixLength))
                return;

            const bool nativeDbc =
                length > prefixLength + dbcExtensionLength &&
                EqualsIgnoreCase(entry + length - dbcExtensionLength, DBC_EXTENSION, dbcExtensionLength);
            const bool customDbc =
                length > prefixLength + cdbcExtensionLength &&
                EqualsIgnoreCase(entry + length - cdbcExtensionLength, CDBC_EXTENSION, cdbcExtensionLength);
            if (!nativeDbc && !customDbc)
                return;

            const size_t extensionLength = nativeDbc ? dbcExtensionLength : cdbcExtensionLength;
            const size_t stemEnd         = length - extensionLength;
            size_t stemStart             = stemEnd;
            while (stemStart > 0 && entry[stemStart - 1] != '\\' && entry[stemStart - 1] != '/')
                --stemStart;
            const char* stem        = entry + stemStart;
            const size_t stemLength = stemEnd - stemStart;

            if (customDbc)
            {
                const std::string name(stem, stemLength);
                if (CDBCRegistry::Reload(name.c_str()))
                    LOG_INFO << "ContentStreaming: refreshed custom CDBC name=" << name;
                else
                    LOG_DEBUG << "ContentStreaming: streamed CDBC is not registered name=" << name;
                return;
            }

            if (!refreshNativeDbcs)
                return;

            if (stemLength >= 5 && EqualsIgnoreCase(stem, "spell", 5))
                result.spellDataChanged = true;
            if (stemLength >= 11 && EqualsIgnoreCase(stem, "achievement", 11))
                result.achievementDataChanged = true;

            ApplyNativeDbc(archive, std::string(entry, length), stem, stemLength, state.characterComponentChanged,
                           state.groundEffectDoodadChanged);
        }

        void ClassifyEntry(const char* entry, size_t length, RefreshResult& result)
        {
            static constexpr char INTERFACE_PREFIX[]      = "interface";
            static constexpr const char* ART_EXTENSIONS[] = {".m2", ".mdx", ".skin", ".blp", ".anim"};
            constexpr size_t interfaceLength              = sizeof(INTERFACE_PREFIX) - 1;

            if (!result.interfaceFiles && ContainsIgnoreCase(entry, length, INTERFACE_PREFIX, interfaceLength))
                result.interfaceFiles = true;

            if (result.artFiles ||
                (length >= interfaceLength && EqualsIgnoreCase(entry, INTERFACE_PREFIX, interfaceLength)))
                return;

            for (const char* extension : ART_EXTENSIONS)
            {
                const size_t extensionLength = std::strlen(extension);
                if (length > extensionLength &&
                    EqualsIgnoreCase(entry + length - extensionLength, extension, extensionLength))
                {
                    result.artFiles = true;
                    return;
                }
            }
        }

        void FinishRefresh(const RefreshState& state)
        {
            if (state.characterComponentChanged)
                DBCPatch::RefreshDerivedState("CharSections");
            if (state.groundEffectDoodadChanged)
                DBCPatch::RefreshDerivedState("GroundEffectDoodad");
        }

        void EnumerateListfile(void* archive, RefreshResult& result, bool refreshNativeDbcs, bool classifyEntries)
        {
            std::vector<uint8_t> listfile;
            if (!ClientArchive::ReadWholeFile(static_cast<System_SFile::ARCHIVEREC*>(archive), "(listfile)", listfile))
            {
                LOG_WARN << "ContentStreaming: mounted archive has no readable listfile";
                return;
            }

            RefreshState state;

            const char* current = reinterpret_cast<const char*>(listfile.data());
            const char* end     = current + listfile.size();
            while (current < end)
            {
                const char* lineEnd = current;
                while (lineEnd < end && *lineEnd != '\r' && *lineEnd != '\n')
                    ++lineEnd;
                const size_t length = static_cast<size_t>(lineEnd - current);

                if (classifyEntries)
                    ClassifyEntry(current, length, result);
                ProcessDbcEntry(archive, current, length, result, refreshNativeDbcs, state);

                current = lineEnd;
                while (current < end && (*current == '\r' || *current == '\n'))
                    ++current;
            }

            FinishRefresh(state);
        }
    } // namespace

    RefreshResult RefreshFromArchive(void* archive, bool refreshNativeDbcs)
    {
        RefreshResult result;
        if (!archive)
            return result;

        EnumerateListfile(archive, result, refreshNativeDbcs, true);
        LOG_DEBUG << "ContentStreaming: DBC refresh complete spell=" << (result.spellDataChanged ? 1 : 0)
                  << " achievement=" << (result.achievementDataChanged ? 1 : 0)
                  << " interface=" << (result.interfaceFiles ? 1 : 0) << " art=" << (result.artFiles ? 1 : 0);
        return result;
    }

    RefreshResult RefreshMembersFromArchive(void* archive, const std::vector<std::string>& memberPaths,
                                            bool refreshNativeDbcs, bool refreshAllDbcs)
    {
        RefreshResult result;
        if (!archive)
            return result;

        RefreshState state;
        for (std::string memberPath : memberPaths)
        {
            std::replace(memberPath.begin(), memberPath.end(), '/', '\\');
            ClassifyEntry(memberPath.c_str(), memberPath.size(), result);
            if (!refreshAllDbcs)
                ProcessDbcEntry(archive, memberPath.c_str(), memberPath.size(), result, refreshNativeDbcs, state);
        }
        if (refreshAllDbcs)
            EnumerateListfile(archive, result, refreshNativeDbcs, false);
        else
            FinishRefresh(state);
        LOG_DEBUG << "ContentStreaming: targeted DBC refresh complete members=" << memberPaths.size()
                  << " dbcMode=" << (refreshAllDbcs ? "all" : "changed")
                  << " spell=" << (result.spellDataChanged ? 1 : 0)
                  << " achievement=" << (result.achievementDataChanged ? 1 : 0)
                  << " interface=" << (result.interfaceFiles ? 1 : 0) << " art=" << (result.artFiles ? 1 : 0);
        return result;
    }
} // namespace DbcFromMpq
