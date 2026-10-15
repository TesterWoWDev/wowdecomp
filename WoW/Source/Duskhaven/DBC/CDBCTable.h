#pragma once

#include "WoW/Source/Duskhaven/DBC/CDBC.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

template <typename Row, typename Key = uint32_t>
class CDBCTable
{
public:
    using KeyFn = Key (*)(const Row&);
    using UnpackFn = void (*)(Row& row, const uint32_t* diskCols, const char* stringBase);

    CDBCTable(const char* name, KeyFn keyFn, UnpackFn unpackFn = nullptr)
        : m_name(name), m_keyFn(keyFn), m_unpackFn(unpackFn)
    {
    }

    ~CDBCTable()
    {
        Unload();
    }

    bool Load()
    {
        if (m_loaded)
            return true;

        CDBC db;
        if (!db.LoadDB(m_name))
            return false;

        m_rows.resize(db.numRows);
        m_index.clear();
        m_index.reserve(db.numRows);

        m_stringTable = db.stringTable;
        db.stringTable = nullptr; // Take ownership so db destructor doesn't free it

        const uint32_t numCols = db.numColumns;
        const uint32_t* rawData = static_cast<const uint32_t*>(db.rawRows);
        const char* strBase = static_cast<const char*>(m_stringTable);

        for (uint32_t i = 0; i < db.numRows; ++i)
        {
            const uint32_t* rowCols = rawData ? (rawData + i * numCols) : nullptr;
            if (m_unpackFn && rowCols)
            {
                m_unpackFn(m_rows[i], rowCols, strBase);
            }
            else if (rowCols)
            {
                size_t copyBytes = sizeof(Row) < (numCols * 4) ? sizeof(Row) : (numCols * 4);
                std::memcpy(&m_rows[i], rowCols, copyBytes);
            }

            if (m_keyFn)
            {
                m_index[m_keyFn(m_rows[i])] = i;
            }
        }

        m_loaded = true;
        return true;
    }

    bool Reload()
    {
        Unload();
        return Load();
    }

    void Unload()
    {
        if (m_stringTable)
        {
            SMemFree(m_stringTable, __FILE__, __LINE__, 0);
            m_stringTable = nullptr;
        }
        m_rows.clear();
        m_index.clear();
        m_dynamicRows.clear();
        m_loaded = false;
    }

    Row* Get(const Key& key)
    {
        auto dynIt = m_dynamicRows.find(key);
        if (dynIt != m_dynamicRows.end())
            return dynIt->second.get();

        auto it = m_index.find(key);
        if (it != m_index.end() && it->second < m_rows.size())
            return &m_rows[it->second];

        return nullptr;
    }

    const Row* Get(const Key& key) const
    {
        return const_cast<CDBCTable*>(this)->Get(key);
    }

    Row* GetRowByIndex(uint32_t index)
    {
        return index < m_rows.size() ? &m_rows[index] : nullptr;
    }

    const Row* GetRowByIndex(uint32_t index) const
    {
        return const_cast<CDBCTable*>(this)->GetRowByIndex(index);
    }

    uint32_t GetNumRows() const
    {
        return static_cast<uint32_t>(m_rows.size());
    }

    void AddRow(const Key& key, const Row& row)
    {
        m_dynamicRows[key] = std::make_unique<Row>(row);
    }

    void EraseRow(const Key& key)
    {
        m_dynamicRows.erase(key);
    }

    template <typename Callback>
    void ForEachRow(Callback&& callback) const
    {
        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            if (m_keyFn && m_dynamicRows.find(m_keyFn(m_rows[i])) != m_dynamicRows.end())
                continue;
            callback(&m_rows[i]);
        }
        for (const auto& pair : m_dynamicRows)
        {
            callback(pair.second.get());
        }
    }

    bool IsLoaded() const { return m_loaded; }

private:
    const char* m_name;
    KeyFn m_keyFn;
    UnpackFn m_unpackFn;
    std::vector<Row> m_rows;
    std::unordered_map<Key, uint32_t> m_index;
    std::unordered_map<Key, std::unique_ptr<Row>> m_dynamicRows;
    void* m_stringTable = nullptr;
    bool m_loaded = false;
};
