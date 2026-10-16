#pragma once
#include <windows.h>
#include <winioctl.h>

namespace Patcher::Filesystem
{
    // Wine exposes Unix filesystem boundaries as mount reparse points, but
    // they have no Windows reparse payload. Never accept symlinks or junctions.
    inline bool IsWineHostMount(const wchar_t* path)
    {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll || !GetProcAddress(ntdll, "wine_get_version"))
            return false;
        WIN32_FIND_DATAW entry{};
        HANDLE search = FindFirstFileW(path, &entry);
        if (search == INVALID_HANDLE_VALUE)
            return false;
        FindClose(search);
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            !(entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
            entry.dwReserved0 != IO_REPARSE_TAG_MOUNT_POINT)
            return false;
        HANDLE file = CreateFileW(path, FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        FILE_ATTRIBUTE_TAG_INFO info{};
        const bool mount = GetFileInformationByHandleEx(file, FileAttributeTagInfo, &info, sizeof(info)) &&
            (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) && info.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT;
        BYTE data[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
        DWORD bytes = 0;
        const BOOL hasPayload = DeviceIoControl(file, FSCTL_GET_REPARSE_POINT,
            nullptr, 0, data, sizeof(data), &bytes, nullptr);
        const DWORD error = GetLastError();
        CloseHandle(file);
        return mount && !hasPayload && error == ERROR_NOT_A_REPARSE_POINT;
    }
}
