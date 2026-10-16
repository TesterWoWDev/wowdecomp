#include <Patcher/PatcherFilesystem.h>
#include <Patcher/PatcherHash.h>
#include <Patcher/PatcherHttp.h>
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>
#include <winhttp.h>

namespace Patcher::Http
{
    namespace
    {
        std::string LastHttpError(const char* prefix);

        struct Handle
        {
            HINTERNET value = nullptr;

            Handle() = default;
            explicit Handle(HINTERNET handle) : value(handle) {}

            ~Handle()
            {
                if (value)
                    WinHttpCloseHandle(value);
            }

            Handle(const Handle&)            = delete;
            Handle& operator=(const Handle&) = delete;

            operator HINTERNET() const
            {
                return value;
            }
            bool IsValid() const
            {
                return value != nullptr;
            }
        };

        constexpr unsigned int MaximumAttempts = 4;

        bool DisableRedirects(HINTERNET request, const std::string& url, std::string& error)
        {
            DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            if (WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)))
                return true;

            error = LastHttpError(("failed to set redirect policy for " + url).c_str());
            return false;
        }

        class Transport
        {
          public:
            Transport()
                : m_session(WinHttpOpen(L"DuskhavenContentStreaming/1.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0))
            {
                if (!m_session.IsValid())
                    m_sessionError = GetLastError();
                if (m_session.IsValid())
                    WinHttpSetTimeouts(m_session, 5000, 5000, 15000, 30000);
            }

            HINTERNET Connection(const std::wstring& host, INTERNET_PORT port, bool secure, std::string& error)
            {
                if (!m_session.IsValid())
                {
                    SetLastError(m_sessionError);
                    error = LastHttpError("failed to open WinHTTP session");
                    return nullptr;
                }

                const std::wstring key = (secure ? L"https://" : L"http://") + ToLowerWide(host) + L":" +
                                         std::to_wstring(port);
                std::lock_guard<std::mutex> lock(m_mutex);
                const auto existing = m_connections.find(key);
                if (existing != m_connections.end())
                    return existing->second->value;

                auto connection = std::make_unique<Handle>(WinHttpConnect(m_session, host.c_str(), port, 0));
                if (!connection->IsValid())
                {
                    error = LastHttpError("failed to create WinHTTP connection");
                    return nullptr;
                }

                HINTERNET value = connection->value;
                m_connections.emplace(key, std::move(connection));
                return value;
            }

          private:
            static std::wstring ToLowerWide(std::wstring value)
            {
                std::transform(value.begin(), value.end(), value.begin(),
                               [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
                return value;
            }

            Handle m_session;
            DWORD m_sessionError = ERROR_SUCCESS;
            std::mutex m_mutex;
            std::map<std::wstring, std::unique_ptr<Handle>> m_connections;
        };

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool StartsWithHttpScheme(const std::string& url)
        {
            const std::string lower = ToLowerAscii(url);
            return lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0;
        }

        struct UrlOrigin
        {
            INTERNET_SCHEME scheme = static_cast<INTERNET_SCHEME>(0);
            std::wstring host;
            INTERNET_PORT port = 0;
        };

        bool Utf8ToWide(const std::string& input, std::wstring& out, std::string& error);

        bool ParseUrlOrigin(const std::string& url, UrlOrigin& out)
        {
            if (url.empty() || url.size() > 2048 || url.find('@') != std::string::npos ||
                url.find('\\') != std::string::npos || url.find('#') != std::string::npos ||
                std::any_of(url.begin(), url.end(), [](unsigned char c) { return c <= 0x20 || c >= 0x7F; }))
                return false;
            std::wstring wideUrl;
            std::string error;
            if (!Utf8ToWide(url, wideUrl, error))
                return false;

            URL_COMPONENTSW components  = {};
            components.dwStructSize     = sizeof(components);
            components.dwSchemeLength   = static_cast<DWORD>(-1);
            components.dwHostNameLength = static_cast<DWORD>(-1);
            if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components) ||
                (components.nScheme != INTERNET_SCHEME_HTTP && components.nScheme != INTERNET_SCHEME_HTTPS) ||
                components.dwHostNameLength == 0)
            {
                return false;
            }

            out.scheme = components.nScheme;
            out.host.assign(components.lpszHostName, components.dwHostNameLength);
            std::transform(out.host.begin(), out.host.end(), out.host.begin(),
                           [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
            out.port = components.nPort;
            return true;
        }

        bool Utf8ToWide(const std::string& input, std::wstring& out, std::string& error)
        {
            if (input.empty())
            {
                out.clear();
                return true;
            }

            int length = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);
            if (length <= 0)
            {
                length = MultiByteToWideChar(CP_ACP, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);
                if (length <= 0)
                {
                    error = Filesystem::LastErrorString("failed to convert URL to wide string");
                    return false;
                }

                out.resize(static_cast<size_t>(length));
                MultiByteToWideChar(CP_ACP, 0, input.c_str(), static_cast<int>(input.size()), &out[0], length);
                return true;
            }

            out.resize(static_cast<size_t>(length));
            MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), &out[0], length);
            return true;
        }

        bool SplitOriginAndPath(const std::string& url, std::string& origin, std::string& path, std::string& error)
        {
            const size_t scheme = url.find("://");
            if (scheme == std::string::npos)
            {
                error = "URL is missing scheme: " + url;
                return false;
            }

            const size_t authority = scheme + 3;
            const size_t pathStart = url.find('/', authority);
            if (pathStart == std::string::npos)
            {
                origin = url;
                path   = "/";
                return true;
            }

            origin = url.substr(0, pathStart);
            path   = url.substr(pathStart);
            if (path.empty())
                path = "/";
            return true;
        }

        std::string NormalizePathSegments(const std::string& path)
        {
            std::vector<std::string> segments;
            std::string current;
            const bool absolute = !path.empty() && path[0] == '/';

            for (char c : path)
            {
                if (c == '/')
                {
                    if (current == "..")
                    {
                        if (!segments.empty())
                            segments.pop_back();
                    }
                    else if (!current.empty() && current != ".")
                    {
                        segments.push_back(current);
                    }
                    current.clear();
                    continue;
                }

                current.push_back(c);
            }

            if (current == "..")
            {
                if (!segments.empty())
                    segments.pop_back();
            }
            else if (!current.empty() && current != ".")
            {
                segments.push_back(current);
            }

            std::ostringstream stream;
            if (absolute)
                stream << "/";
            for (size_t i = 0; i < segments.size(); ++i)
            {
                if (i != 0)
                    stream << "/";
                stream << segments[i];
            }
            return stream.str();
        }

        std::string LastHttpError(const char* prefix)
        {
            return Filesystem::LastErrorString(prefix);
        }

        Transport& SharedTransport()
        {
            static Transport transport;
            return transport;
        }

        bool IsTransientHttpStatus(DWORD status)
        {
            return status == 408 || status == 425 || status == 429 || status == 500 || status == 502 ||
                   status == 503 || status == 504;
        }

        bool IsTransientWinHttpError(DWORD code)
        {
            switch (code)
            {
                case ERROR_WINHTTP_TIMEOUT:
                case ERROR_WINHTTP_CANNOT_CONNECT:
                case ERROR_WINHTTP_CONNECTION_ERROR:
                case ERROR_WINHTTP_NAME_NOT_RESOLVED:
                case ERROR_WINHTTP_RESEND_REQUEST:
                case ERROR_WINHTTP_INTERNAL_ERROR:
                case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:
                    return true;
                default:
                    return false;
            }
        }

        bool QueryHeaderString(HINTERNET request, DWORD query, std::wstring& value)
        {
            DWORD bytes = 0;
            if (WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &bytes,
                                    WINHTTP_NO_HEADER_INDEX) ||
                GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                return false;

            std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
            if (!WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, buffer.data(), &bytes,
                                     WINHTTP_NO_HEADER_INDEX))
                return false;
            value.assign(buffer.data());
            return true;
        }

        DWORD RetryDelay(HINTERNET request, DWORD status, unsigned int failedAttempt)
        {
            DWORD delay = 250u << (std::min)(failedAttempt, 3u);
            if ((status == 429 || status == 503) && request)
            {
                std::wstring retryAfter;
                if (QueryHeaderString(request, WINHTTP_QUERY_RETRY_AFTER, retryAfter) && !retryAfter.empty() &&
                    std::all_of(retryAfter.begin(), retryAfter.end(), [](wchar_t c) { return std::iswdigit(c) != 0; }))
                {
                    uint64_t seconds = 0;
                    for (wchar_t c : retryAfter)
                        seconds = (std::min)(seconds * 10 + static_cast<uint64_t>(c - L'0'), uint64_t(10));
                    delay = static_cast<DWORD>(seconds * 1000);
                }
            }

            const DWORD jitter = (GetTickCount() ^ GetCurrentThreadId()) % 126;
            return (std::min)(delay + jitter, DWORD(10000));
        }

        bool WaitForRetry(unsigned int failedAttempt, const std::function<bool()>& shouldContinue = {},
                          HINTERNET request = nullptr, DWORD status = 0)
        {
            const DWORD delay = RetryDelay(request, status, failedAttempt);
            DWORD waited      = 0;
            while (waited < delay)
            {
                if (shouldContinue && !shouldContinue())
                    return false;
                const DWORD slice = (std::min)(DWORD(50), delay - waited);
                Sleep(slice);
                waited += slice;
            }
            return !shouldContinue || shouldContinue();
        }

        bool ParseUnsignedWide(const std::wstring& value, size_t begin, size_t end, uint64_t& out)
        {
            if (begin >= end)
                return false;
            out = 0;
            for (size_t index = begin; index < end; ++index)
            {
                if (!std::iswdigit(value[index]))
                    return false;
                const uint64_t digit = static_cast<uint64_t>(value[index] - L'0');
                if (out > (UINT64_MAX - digit) / 10)
                    return false;
                out = out * 10 + digit;
            }
            return true;
        }

        bool ValidateContentRange(HINTERNET request, uint64_t expectedStart, uint64_t expectedTotal,
                                  std::string& error)
        {
            std::wstring value;
            if (!QueryHeaderString(request, WINHTTP_QUERY_CONTENT_RANGE, value))
            {
                error = "HTTP 206 response is missing Content-Range";
                return false;
            }

            constexpr wchar_t Prefix[]    = L"bytes ";
            constexpr size_t PrefixLength = 6;
            const size_t dash             = value.find(L'-', PrefixLength);
            const size_t slash = dash == std::wstring::npos ? std::wstring::npos : value.find(L'/', dash + 1);
            uint64_t start = 0;
            uint64_t end   = 0;
            if (value.rfind(Prefix, 0) != 0 || dash == std::wstring::npos || slash == std::wstring::npos ||
                !ParseUnsignedWide(value, PrefixLength, dash, start) ||
                !ParseUnsignedWide(value, dash + 1, slash, end) || start != expectedStart || end < start)
            {
                error = "HTTP 206 response has an invalid Content-Range";
                return false;
            }

            if (value.compare(slash + 1, std::wstring::npos, L"*") == 0)
            {
                if (expectedTotal > 0)
                {
                    error = "HTTP 206 response did not include the expected total size";
                    return false;
                }
            }
            else
            {
                uint64_t total = 0;
                if (!ParseUnsignedWide(value, slash + 1, value.size(), total) || total <= end ||
                    (expectedTotal > 0 && total != expectedTotal))
                {
                    error = "HTTP 206 response has an unexpected Content-Range size";
                    return false;
                }
            }
            return true;
        }

        bool HashFilePrefix(const std::string& path, uint64_t offset, uint64_t bytes, Hash::Md5& hash,
                            const std::function<bool(uint64_t, uint64_t)>& onProgress, uint64_t expectedSize,
                            std::string& error)
        {
            if (bytes == 0)
                return true;

            HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                error = LastHttpError(("failed to open partial download " + path).c_str());
                return false;
            }

            if (offset != 0)
            {
                LARGE_INTEGER position = {};
                position.QuadPart       = static_cast<LONGLONG>(offset);
                if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN))
                {
                    error = LastHttpError(("failed to seek partial download " + path).c_str());
                    CloseHandle(file);
                    return false;
                }
            }

            std::vector<unsigned char> buffer(1024 * 1024);
            uint64_t remaining = bytes;
            while (remaining > 0)
            {
                const DWORD requested = static_cast<DWORD>((std::min)(remaining, uint64_t(buffer.size())));
                DWORD read            = 0;
                if (!ReadFile(file, buffer.data(), requested, &read, nullptr) || read == 0)
                {
                    error = LastHttpError(("failed to hash partial download " + path).c_str());
                    CloseHandle(file);
                    return false;
                }
                if (!hash.Update(buffer.data(), read, error))
                {
                    CloseHandle(file);
                    return false;
                }
                remaining -= read;
                // The caller already reports the saved prefix as downloaded.
                // Repeating that value polls cancellation without moving progress backward.
                if (onProgress && !onProgress(bytes, expectedSize))
                {
                    error = "download canceled";
                    CloseHandle(file);
                    return false;
                }
            }

            CloseHandle(file);
            return true;
        }

        bool ExistingFileContainsRange(const std::string& path, uint64_t offset, uint64_t bytes, bool exactSize)
        {
            HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return false;
            LARGE_INTEGER size = {};
            const bool validSize = GetFileSizeEx(file, &size) && size.QuadPart >= 0;
            const uint64_t required = offset <= (std::numeric_limits<uint64_t>::max)() - bytes ? offset + bytes : 0;
            const bool matches = validSize && required != 0 &&
                                 (exactSize ? static_cast<uint64_t>(size.QuadPart) == required :
                                              static_cast<uint64_t>(size.QuadPart) >= required);
            CloseHandle(file);
            return matches;
        }
    } // namespace

    bool IsHttpUrl(const std::string& url)
    {
        return StartsWithHttpScheme(url);
    }

    bool IsSecureOrLoopbackUrl(const std::string& url)
    {
        UrlOrigin origin;
        if (!ParseUrlOrigin(url, origin))
            return false;
        if (origin.scheme == INTERNET_SCHEME_HTTPS)
            return true;

        return origin.host == L"localhost" || origin.host == L"127.0.0.1" || origin.host == L"::1";
    }

    bool HaveSameOrigin(const std::string& first, const std::string& second)
    {
        UrlOrigin firstOrigin;
        UrlOrigin secondOrigin;
        return ParseUrlOrigin(first, firstOrigin) && ParseUrlOrigin(second, secondOrigin) &&
               firstOrigin.scheme == secondOrigin.scheme && firstOrigin.host == secondOrigin.host &&
               firstOrigin.port == secondOrigin.port;
    }

    bool ResolveUrl(const std::string& baseUrl, const std::string& target, std::string& out, std::string& error)
    {
        if (target.empty())
        {
            error = "URL target is empty";
            return false;
        }

        if (IsHttpUrl(target))
        {
            out = target;
            return true;
        }

        if (!IsHttpUrl(baseUrl))
        {
            error = "base URL is not http/https: " + baseUrl;
            return false;
        }

        std::string origin;
        std::string basePath;
        if (!SplitOriginAndPath(baseUrl, origin, basePath, error))
            return false;

        if (target.size() >= 2 && target[0] == '/' && target[1] == '/')
        {
            const std::string scheme = baseUrl.substr(0, baseUrl.find(':'));
            out                      = scheme + ":" + target;
            return true;
        }

        if (!target.empty() && target[0] == '/')
        {
            out = origin + NormalizePathSegments(target);
            return true;
        }

        const size_t slash          = basePath.rfind('/');
        const std::string directory = slash == std::string::npos ? "/" : basePath.substr(0, slash + 1);
        out                         = origin + NormalizePathSegments(directory + target);
        return true;
    }

    bool DownloadString(const std::string& url, size_t maxBytes, std::string& out, std::string& error)
    {
        error.clear();
        if (!IsSecureOrLoopbackUrl(url))
        {
            error = "download URL must use HTTPS without credentials (HTTP is allowed only for loopback testing)";
            return false;
        }

        std::wstring wideUrl;
        if (!Utf8ToWide(url, wideUrl, error))
            return false;

        URL_COMPONENTSW components   = {};
        components.dwStructSize      = sizeof(components);
        components.dwSchemeLength    = static_cast<DWORD>(-1);
        components.dwHostNameLength  = static_cast<DWORD>(-1);
        components.dwUrlPathLength   = static_cast<DWORD>(-1);
        components.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components))
        {
            error = LastHttpError(("failed to parse URL " + url).c_str());
            return false;
        }

        if (components.nScheme != INTERNET_SCHEME_HTTP && components.nScheme != INTERNET_SCHEME_HTTPS)
        {
            error = "URL scheme is not supported: " + url;
            return false;
        }

        std::wstring host(components.lpszHostName, components.dwHostNameLength);
        std::wstring object(components.lpszUrlPath, components.dwUrlPathLength);
        if (components.dwExtraInfoLength > 0)
            object.append(components.lpszExtraInfo, components.dwExtraInfoLength);
        if (object.empty())
            object = L"/";

        const bool secure    = components.nScheme == INTERNET_SCHEME_HTTPS;
        HINTERNET connection = SharedTransport().Connection(host, components.nPort, secure, error);
        if (!connection)
            return false;

        const DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
        for (unsigned int attempt = 0; attempt < MaximumAttempts; ++attempt)
        {
            Handle request(WinHttpOpenRequest(connection, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
            if (!request.IsValid())
            {
                error = LastHttpError(("failed to create request for " + url).c_str());
                return false;
            }
            if (!DisableRedirects(request, url, error))
                return false;

            if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
                !WinHttpReceiveResponse(request, nullptr))
            {
                const DWORD code = GetLastError();
                error            = LastHttpError(("failed to download " + url).c_str());
                if (attempt + 1 < MaximumAttempts && IsTransientWinHttpError(code) && WaitForRetry(attempt))
                    continue;
                return false;
            }

            DWORD status     = 0;
            DWORD statusSize = sizeof(status);
            if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                     WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX))
            {
                error = LastHttpError(("failed to read HTTP status for " + url).c_str());
                return false;
            }
            if (IsTransientHttpStatus(status) && attempt + 1 < MaximumAttempts)
            {
                if (!WaitForRetry(attempt, {}, request, status))
                    return false;
                continue;
            }
            if (status < 200 || status >= 300)
            {
                std::ostringstream stream;
                stream << "HTTP " << status << " while downloading " << url;
                error = stream.str();
                return false;
            }

            out.clear();
            bool retry = false;
            while (true)
            {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available))
                {
                    const DWORD code = GetLastError();
                    error            = LastHttpError(("failed to query download size for " + url).c_str());
                    retry            = IsTransientWinHttpError(code);
                    break;
                }
                if (available == 0)
                    return true;

                if (maxBytes != 0 && (out.size() > maxBytes || available > maxBytes - out.size()))
                {
                    std::ostringstream stream;
                    stream << "download exceeded limit of " << maxBytes << " bytes: " << url;
                    error = stream.str();
                    return false;
                }

                const size_t offset = out.size();
                out.resize(offset + available);
                DWORD read = 0;
                if (!WinHttpReadData(request, &out[offset], available, &read))
                {
                    const DWORD code = GetLastError();
                    out.resize(offset);
                    error = LastHttpError(("failed to read download body for " + url).c_str());
                    retry = IsTransientWinHttpError(code);
                    break;
                }
                out.resize(offset + read);
            }

            if (!retry || attempt + 1 >= MaximumAttempts || !WaitForRetry(attempt))
                return false;
        }

        return false;
    }

    bool DownloadFileResumable(const std::string& url, const std::string& destinationPath, const ResumeOptions& options,
                               std::string& error)
    {
        error.clear();
        if (!IsSecureOrLoopbackUrl(url))
        {
            error = "download URL must use HTTPS without credentials (HTTP is allowed only for loopback testing)";
            return false;
        }

        std::wstring wideUrl;
        if (!Utf8ToWide(url, wideUrl, error))
            return false;

        URL_COMPONENTSW components   = {};
        components.dwStructSize      = sizeof(components);
        components.dwSchemeLength    = static_cast<DWORD>(-1);
        components.dwHostNameLength  = static_cast<DWORD>(-1);
        components.dwUrlPathLength   = static_cast<DWORD>(-1);
        components.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components))
        {
            error = LastHttpError(("failed to parse URL " + url).c_str());
            return false;
        }
        if (options.destinationOffset > static_cast<uint64_t>((std::numeric_limits<LONGLONG>::max)()) ||
            options.expectedSize > static_cast<uint64_t>((std::numeric_limits<LONGLONG>::max)()) -
                                       options.destinationOffset ||
            (options.expectedSize != 0 && options.resumeFrom > options.expectedSize) ||
            (options.preserveDestination && options.expectedSize == 0))
        {
            error = "download destination range overflow";
            return false;
        }
        if (options.destinationOffset != 0 && !options.preserveDestination)
        {
            error = "download destination offset requires preserved output";
            return false;
        }
        if (components.nScheme != INTERNET_SCHEME_HTTP && components.nScheme != INTERNET_SCHEME_HTTPS)
        {
            error = "URL scheme is not supported: " + url;
            return false;
        }

        std::wstring host(components.lpszHostName, components.dwHostNameLength);
        std::wstring object(components.lpszUrlPath, components.dwUrlPathLength);
        if (components.dwExtraInfoLength > 0)
            object.append(components.lpszExtraInfo, components.dwExtraInfoLength);
        if (object.empty())
            object = L"/";

        const bool secure    = components.nScheme == INTERNET_SCHEME_HTTPS;
        HINTERNET connection = SharedTransport().Connection(host, components.nPort, secure, error);
        if (!connection)
            return false;
        const DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
        uint64_t resumeFrom = options.resumeFrom;
        if (resumeFrom > 0 &&
            !ExistingFileContainsRange(destinationPath, options.destinationOffset, resumeFrom,
                                       !options.preserveDestination))
            resumeFrom = 0;

        std::unique_ptr<Hash::Md5> hash;
        const auto resetHash = [&](uint64_t prefixSize)
        {
            hash.reset();
            if (options.expectedMd5.empty())
                return true;
            hash = std::make_unique<Hash::Md5>();
            return hash->Begin(error) &&
                   HashFilePrefix(destinationPath, options.destinationOffset, prefixSize, *hash, options.onProgress,
                                  options.expectedSize, error);
        };
        if (!resetHash(resumeFrom))
            return false;

        std::vector<unsigned char> buffer(1024 * 1024);
        unsigned int failedAttempts = 0;
        bool resetAfterRangeFailure = false;
        while (failedAttempts < MaximumAttempts)
        {
            Handle request(WinHttpOpenRequest(connection, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
            if (!request.IsValid())
            {
                error = LastHttpError(("failed to create request for " + url).c_str());
                return false;
            }
            if (!DisableRedirects(request, url, error))
                return false;

            if (resumeFrom > 0)
            {
                const std::wstring rangeHeader = L"Range: bytes=" + std::to_wstring(resumeFrom) + L"-\r\n";
                if (!WinHttpAddRequestHeaders(request, rangeHeader.c_str(), static_cast<DWORD>(-1),
                                              WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
                {
                    error = LastHttpError(("failed to add Range header for " + url).c_str());
                    return false;
                }
            }

            if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
                !WinHttpReceiveResponse(request, nullptr))
            {
                const DWORD code = GetLastError();
                error            = LastHttpError(("failed to download " + url).c_str());
                ++failedAttempts;
                const auto continueDownload = [&]()
                { return !options.onProgress || options.onProgress(resumeFrom, options.expectedSize); };
                if (failedAttempts < MaximumAttempts && IsTransientWinHttpError(code) &&
                    WaitForRetry(failedAttempts - 1, continueDownload))
                    continue;
                if (options.onProgress && !continueDownload())
                    error = "download canceled";
                return false;
            }

            DWORD status     = 0;
            DWORD statusSize = sizeof(status);
            if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                     WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX))
            {
                error = LastHttpError(("failed to read HTTP status for " + url).c_str());
                return false;
            }

            if (status == 416 && resumeFrom > 0 && !resetAfterRangeFailure)
            {
                resumeFrom            = 0;
                resetAfterRangeFailure = true;
                if (!resetHash(0))
                    return false;
                continue;
            }
            if (IsTransientHttpStatus(status))
            {
                ++failedAttempts;
                const auto continueDownload = [&]()
                { return !options.onProgress || options.onProgress(resumeFrom, options.expectedSize); };
                if (failedAttempts < MaximumAttempts &&
                    WaitForRetry(failedAttempts - 1, continueDownload, request, status))
                    continue;
                if (options.onProgress && !continueDownload())
                    error = "download canceled";
                else
                    error = "HTTP " + std::to_string(status) + " while downloading " + url;
                return false;
            }
            if (status < 200 || status >= 300 || (resumeFrom > 0 && status != 200 && status != 206))
            {
                std::ostringstream stream;
                stream << "HTTP " << status << " while downloading " << url;
                error = stream.str();
                return false;
            }
            if (status == 206 && !ValidateContentRange(request, resumeFrom, options.expectedSize, error))
                return false;

            if (options.expectedSize > 0)
            {
                std::wstring contentLength;
                if (QueryHeaderString(request, WINHTTP_QUERY_CONTENT_LENGTH, contentLength))
                {
                    uint64_t responseSize = 0;
                    const uint64_t responseOffset = status == 206 ? resumeFrom : 0;
                    if (!ParseUnsignedWide(contentLength, 0, contentLength.size(), responseSize) ||
                        responseOffset > options.expectedSize || responseSize > options.expectedSize - responseOffset)
                    {
                        error = "HTTP response exceeds the expected download range";
                        return false;
                    }
                }
            }

            const bool resumed = resumeFrom > 0 && status == 206;
            if (!resumed && resumeFrom > 0)
            {
                resumeFrom = 0;
                if (!resetHash(0))
                    return false;
            }

            const DWORD shareMode = options.preserveDestination ? FILE_SHARE_READ | FILE_SHARE_WRITE : 0;
            const DWORD disposition = options.preserveDestination || resumed ? OPEN_ALWAYS : CREATE_ALWAYS;
            HANDLE file = CreateFileA(destinationPath.c_str(), GENERIC_WRITE, shareMode, nullptr, disposition,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                error = LastHttpError(("failed to open file " + destinationPath).c_str());
                return false;
            }
            if (options.preserveDestination || resumed)
            {
                LARGE_INTEGER offset = {};
                offset.QuadPart = static_cast<LONGLONG>(options.destinationOffset + (resumed ? resumeFrom : 0));
                const bool positioned = SetFilePointerEx(file, offset, nullptr, FILE_BEGIN) != FALSE;
                const bool truncated  = options.preserveDestination || SetEndOfFile(file) != FALSE;
                if (!positioned || !truncated)
                {
                    error = LastHttpError(("failed to seek resume file " + destinationPath).c_str());
                    CloseHandle(file);
                    return false;
                }
            }

            uint64_t totalWritten = resumeFrom;
            bool retry            = false;
            DWORD retryCode       = ERROR_SUCCESS;
            while (true)
            {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available))
                {
                    retryCode = GetLastError();
                    error     = LastHttpError(("failed to query download size for " + url).c_str());
                    retry     = IsTransientWinHttpError(retryCode);
                    break;
                }
                if (available == 0)
                    break;
                // Keep both allocation and writes bounded, especially when
                // parallel chunks share adjacent ranges of one destination.
                const DWORD requested = static_cast<DWORD>((std::min)(static_cast<size_t>(available), buffer.size()));
                DWORD read = 0;
                if (!WinHttpReadData(request, buffer.data(), requested, &read))
                {
                    retryCode = GetLastError();
                    error     = LastHttpError(("failed to read download body for " + url).c_str());
                    retry     = IsTransientWinHttpError(retryCode);
                    break;
                }
                if (read == 0)
                    break;

                if (options.expectedSize > 0 &&
                    (totalWritten > options.expectedSize || read > options.expectedSize - totalWritten))
                {
                    error = "download exceeded expected size for " + url;
                    CloseHandle(file);
                    return false;
                }

                DWORD written = 0;
                if (!WriteFile(file, buffer.data(), read, &written, nullptr))
                {
                    error = LastHttpError(("failed to write file " + destinationPath).c_str());
                    CloseHandle(file);
                    return false;
                }
                if (written != read)
                {
                    std::ostringstream stream;
                    stream << "short write for " << destinationPath << ": expected " << read << " got " << written;
                    error = stream.str();
                    CloseHandle(file);
                    return false;
                }
                if (hash && !hash->Update(buffer.data(), written, error))
                {
                    CloseHandle(file);
                    return false;
                }
                totalWritten += written;

                if (options.onProgress && !options.onProgress(totalWritten, options.expectedSize))
                {
                    error = "download canceled";
                    CloseHandle(file);
                    return false;
                }
                if (options.expectedSize > 0 && totalWritten == options.expectedSize)
                    break;
            }
            CloseHandle(file);
            resumeFrom = totalWritten;

            // If the expected body is complete, let the size and MD5 checks
            // decide the result instead of issuing a Range request at EOF.
            if (retry && options.expectedSize > 0 && totalWritten == options.expectedSize)
                retry = false;

            if (retry)
            {
                ++failedAttempts;
                const auto continueDownload = [&]()
                { return !options.onProgress || options.onProgress(resumeFrom, options.expectedSize); };
                if (failedAttempts < MaximumAttempts && IsTransientWinHttpError(retryCode) &&
                    WaitForRetry(failedAttempts - 1, continueDownload))
                    continue;
                if (options.onProgress && !continueDownload())
                    error = "download canceled";
                return false;
            }

            if (options.expectedSize > 0 && totalWritten < options.expectedSize)
            {
                ++failedAttempts;
                const auto continueDownload = [&]()
                { return !options.onProgress || options.onProgress(resumeFrom, options.expectedSize); };
                if (failedAttempts < MaximumAttempts && WaitForRetry(failedAttempts - 1, continueDownload))
                    continue;
                if (options.onProgress && !continueDownload())
                {
                    error = "download canceled";
                    return false;
                }
            }

            if (options.expectedSize > 0 && totalWritten != options.expectedSize)
            {
                std::ostringstream stream;
                stream << "downloaded file size mismatch for " << url << ": expected " << options.expectedSize
                       << " got " << totalWritten;
                error = stream.str();
                return false;
            }

            if (options.flushToDisk)
            {
                const DWORD completedShare = options.preserveDestination ? FILE_SHARE_READ | FILE_SHARE_WRITE : 0;
                HANDLE completedFile = CreateFileA(destinationPath.c_str(), GENERIC_WRITE, completedShare, nullptr,
                                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (completedFile == INVALID_HANDLE_VALUE || !FlushFileBuffers(completedFile))
                {
                    error = LastHttpError(("failed to flush file " + destinationPath).c_str());
                    if (completedFile != INVALID_HANDLE_VALUE)
                        CloseHandle(completedFile);
                    return false;
                }
                CloseHandle(completedFile);
            }

            if (hash)
            {
                std::string actualMd5;
                if (!hash->Finish(actualMd5, error))
                    return false;
                if (!Filesystem::EqualsIgnoreCase(actualMd5, options.expectedMd5))
                {
                    error = "downloaded file MD5 mismatch for " + url;
                    return false;
                }
            }
            error.clear();
            return true;
        }

        return false;
    }
} // namespace Patcher::Http
