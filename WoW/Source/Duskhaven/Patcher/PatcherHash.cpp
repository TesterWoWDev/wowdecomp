#include "PatcherHash.h"

namespace Patcher::Hash
{
    namespace
    {
        bool NtSucceeded(NTSTATUS status)
        {
            return status >= 0;
        }

        std::string BytesToHex(const std::vector<unsigned char>& bytes)
        {
            static constexpr char hex[] = "0123456789ABCDEF";
            std::string out;
            out.resize(bytes.size() * 2);
            for (size_t i = 0; i < bytes.size(); ++i)
            {
                out[i * 2]     = hex[(bytes[i] >> 4) & 0xF];
                out[i * 2 + 1] = hex[bytes[i] & 0xF];
            }
            return out;
        }
    } // namespace

    Md5::~Md5()
    {
        if (m_hash)
            BCryptDestroyHash(m_hash);
        if (m_algorithm)
            BCryptCloseAlgorithmProvider(m_algorithm, 0);
    }

    bool Md5::Begin(std::string& error)
    {
        NTSTATUS status = BCryptOpenAlgorithmProvider(&m_algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0);
        if (!NtSucceeded(status))
        {
            error = "failed to open MD5 provider";
            return false;
        }

        DWORD objectLength = 0;
        DWORD bytesCopied  = 0;
        status = BCryptGetProperty(m_algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength),
                                   sizeof(objectLength), &bytesCopied, 0);
        if (NtSucceeded(status))
        {
            status = BCryptGetProperty(m_algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&m_hashLength),
                                       sizeof(m_hashLength), &bytesCopied, 0);
        }
        if (!NtSucceeded(status) || objectLength == 0 || m_hashLength == 0)
        {
            error = "failed to query MD5 provider";
            return false;
        }

        m_hashObject.resize(objectLength);
        status = BCryptCreateHash(m_algorithm, &m_hash, m_hashObject.data(), objectLength, nullptr, 0, 0);
        if (!NtSucceeded(status))
        {
            error = "failed to create MD5 hash";
            return false;
        }
        return true;
    }

    bool Md5::Update(const void* data, uint32_t size, std::string& error)
    {
        if (!m_hash)
        {
            error = "MD5 hash has not been initialized";
            return false;
        }

        NTSTATUS status = BCryptHashData(m_hash, static_cast<PUCHAR>(const_cast<void*>(data)), size, 0);
        if (!NtSucceeded(status))
        {
            error = "failed to hash file data";
            return false;
        }
        return true;
    }

    bool Md5::Finish(std::string& outHash, std::string& error)
    {
        if (!m_hash || m_hashLength == 0)
        {
            error = "MD5 hash has not been initialized";
            return false;
        }

        std::vector<unsigned char> hashBytes(m_hashLength);
        NTSTATUS status = BCryptFinishHash(m_hash, hashBytes.data(), m_hashLength, 0);
        if (!NtSucceeded(status))
        {
            error = "failed to finalize MD5 hash";
            return false;
        }

        BCryptDestroyHash(m_hash);
        m_hash  = nullptr;
        outHash = BytesToHex(hashBytes);
        return true;
    }

    Sha256::~Sha256()
    {
        if (m_hash)
            BCryptDestroyHash(m_hash);
        if (m_algorithm)
            BCryptCloseAlgorithmProvider(m_algorithm, 0);
    }

    bool Sha256::Begin(std::string& error)
    {
        NTSTATUS status = BCryptOpenAlgorithmProvider(&m_algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (!NtSucceeded(status))
        {
            error = "failed to open SHA-256 provider";
            return false;
        }

        DWORD objectLength = 0;
        DWORD bytesCopied  = 0;
        status = BCryptGetProperty(m_algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength),
                                   sizeof(objectLength), &bytesCopied, 0);
        if (NtSucceeded(status))
        {
            status = BCryptGetProperty(m_algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&m_hashLength),
                                       sizeof(m_hashLength), &bytesCopied, 0);
        }
        if (!NtSucceeded(status) || objectLength == 0 || m_hashLength == 0)
        {
            error = "failed to query SHA-256 provider";
            return false;
        }

        m_hashObject.resize(objectLength);
        status = BCryptCreateHash(m_algorithm, &m_hash, m_hashObject.data(), objectLength, nullptr, 0, 0);
        if (!NtSucceeded(status))
        {
            error = "failed to create SHA-256 hash";
            return false;
        }

        return true;
    }

    bool Sha256::Update(const void* data, uint32_t size, std::string& error)
    {
        if (!m_hash)
        {
            error = "SHA-256 hash has not been initialized";
            return false;
        }

        NTSTATUS status = BCryptHashData(m_hash, static_cast<PUCHAR>(const_cast<void*>(data)), size, 0);
        if (!NtSucceeded(status))
        {
            error = "failed to hash file data";
            return false;
        }

        return true;
    }

    bool Sha256::Finish(std::string& outHash, std::string& error)
    {
        if (!m_hash || m_hashLength == 0)
        {
            error = "SHA-256 hash has not been initialized";
            return false;
        }

        std::vector<unsigned char> hashBytes(m_hashLength);
        NTSTATUS status = BCryptFinishHash(m_hash, hashBytes.data(), m_hashLength, 0);
        if (!NtSucceeded(status))
        {
            error = "failed to finalize SHA-256 hash";
            return false;
        }

        BCryptDestroyHash(m_hash);
        m_hash  = nullptr;
        outHash = BytesToHex(hashBytes);
        return true;
    }
} // namespace Patcher::Hash
