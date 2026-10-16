#pragma once

#include <Windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Patcher::Hash
{
    class Md5
    {
      public:
        Md5() = default;
        ~Md5();

        Md5(const Md5&)            = delete;
        Md5& operator=(const Md5&) = delete;

        bool Begin(std::string& error);
        bool Update(const void* data, uint32_t size, std::string& error);
        bool Finish(std::string& outHash, std::string& error);

      private:
        BCRYPT_ALG_HANDLE m_algorithm = nullptr;
        BCRYPT_HASH_HANDLE m_hash     = nullptr;
        std::vector<unsigned char> m_hashObject;
        DWORD m_hashLength = 0;
    };

    class Sha256
    {
      public:
        Sha256() = default;
        ~Sha256();

        Sha256(const Sha256&)            = delete;
        Sha256& operator=(const Sha256&) = delete;

        bool Begin(std::string& error);
        bool Update(const void* data, uint32_t size, std::string& error);
        bool Finish(std::string& outHash, std::string& error);

      private:
        BCRYPT_ALG_HANDLE m_algorithm = nullptr;
        BCRYPT_HASH_HANDLE m_hash     = nullptr;
        std::vector<unsigned char> m_hashObject;
        DWORD m_hashLength = 0;
    };
} // namespace Patcher::Hash
