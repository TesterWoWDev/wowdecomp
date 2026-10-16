#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct NativeClientDB;

// Applies streamed DBC records directly to native WowClientDB storage.
class DBCPatch
{
  public:
    static bool ApplyRecords(const char* dbcName, uint32_t recordSize, const std::vector<uint16_t>& strOffsets,
                             const std::vector<uint32_t>& ids, const uint8_t* images, const char* strBlock,
                             uint32_t strBlockSize, bool idsAreComplete = false);

    // Rebuild client-owned indexes after ApplyRecords releases its parse buffers.
    static void RefreshDerivedState(const char* dbcName);

    static bool IsPatchedRow(const void* p);

    // Read-only lookup in the client's own storage, for consumers such as the
    // display -> model lookups a creature preview needs. nullptr when the DBC or
    // the row is absent. Rows are client-owned; never write through this pointer.
    static void* FindRow(const char* dbcName, uint32_t id);

  private:
    struct PatchRange
    {
        uintptr_t begin, end;
    };

    static std::vector<PatchRange>& PatchedRanges();
    static void AddPatchedRange(const void* begin, const void* end);

    static NativeClientDB* FindStorage(const char* name);
    static bool EnsureCapacity(NativeClientDB* db, int wantMin, int wantMax);

    // The client loads FirstRow in file order, which is not always id order.
    static std::vector<int> SnapshotDenseOrder(NativeClientDB* db, uint32_t recordSize, bool inlineStorage);

    static void RebuildDenseArray(NativeClientDB* db, uint32_t recordSize, bool inlineStorage,
                                  const std::vector<int>& order, const uint16_t* sortKey);
    static void RebuildInlineDense(NativeClientDB* db, void** byId, const std::vector<int>& order,
                                   uint32_t recordSize);
    static void RebuildPointerDense(NativeClientDB* db, void** byId, const std::vector<int>& order);
    static void** SlotFor(NativeClientDB* db, uint32_t id);
    static void FixupStrings(void* record, const std::vector<uint16_t>& strOffsets, const char* internedBlob,
                             uint32_t blobSize);
    static void WriteRecord(void* dst, const uint8_t* image, uint32_t recordSize,
                            const std::vector<uint16_t>& strOffsets, const char* blob, uint32_t blobSize);

    // Storages the client post-processes after load: patched rows need the same treatment.
    static void FixupRowForClient(const char* dbcName, void* record);
    static void RefreshDerivedState(const char* dbcName, NativeClientDB* db);
};
