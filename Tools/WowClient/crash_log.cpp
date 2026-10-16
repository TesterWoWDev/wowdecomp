#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>
#include <intrin.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

static char s_logPath[MAX_PATH];
static HANDLE s_console = INVALID_HANDLE_VALUE;
static HANDLE s_logFile = INVALID_HANDLE_VALUE;

static void LogLine(const char* text) {
    fputs(text, stderr);
    fflush(stderr);

    if (s_console != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(s_console, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    }

    if (s_logFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(s_logFile, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
        FlushFileBuffers(s_logFile);
    }
}

extern "C" void CrashLogWrite(const char* text) {
    LogLine(text);
}

static void LogStack(CONTEXT* context);

extern "C" __declspec(selectany) void (*g_pfnCrashLogWriteStack)(const char* header) = nullptr;

extern "C" void CrashLogWriteStack(const char* header) {
    LogLine(header);
    CONTEXT context;
    RtlCaptureContext(&context);
    LogStack(&context);
}

static void OnProcessExit() {
    LogLine("EXIT: atexit triggered (process shutting down)\n");
}

static void ModuleOffset(void* address, char* out, size_t size) {
    HMODULE module = nullptr;
    char path[MAX_PATH] = "?";

    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCSTR>(address), &module) && module) {
        GetModuleFileNameA(module, path, sizeof(path));
        const char* name = strrchr(path, '\\');
        _snprintf_s(out, size, _TRUNCATE, "%s+0x%llX", name ? name + 1 : path, static_cast<unsigned long long>(reinterpret_cast<char*>(address) - reinterpret_cast<char*>(module)));
    } else {
        _snprintf_s(out, size, _TRUNCATE, "%p", address);
    }
}

static void LogStack(CONTEXT* context) {
#if defined(_M_X64)
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, nullptr, TRUE);

    CONTEXT frame = *context;

    for (int depth = 0; depth < 24 && frame.Rip; depth++) {
        char where[MAX_PATH + 32];
        ModuleOffset(reinterpret_cast<void*>(frame.Rip), where, sizeof(where));

        char buffer[sizeof(SYMBOL_INFO) + 256] = {};
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;

        IMAGEHLP_LINE64 source = {};
        source.SizeOfStruct = sizeof(source);
        DWORD lineDisplacement = 0;

        char line[1024];

        if (SymFromAddr(process, frame.Rip, &displacement, symbol)) {
            if (SymGetLineFromAddr64(process, frame.Rip, &lineDisplacement, &source)) {
                _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d %s %s+0x%llX (%s:%lu)\n", depth, where, symbol->Name, static_cast<unsigned long long>(displacement), source.FileName, source.LineNumber);
            } else {
                _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d %s %s+0x%llX\n", depth, where, symbol->Name, static_cast<unsigned long long>(displacement));
            }
        } else {
            _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d %s\n", depth, where);
        }

        LogLine(line);

        __try {
            DWORD64 imageBase = 0;
            PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(frame.Rip, &imageBase, nullptr);

            if (!function) {
                if (IsBadReadPtr(reinterpret_cast<void*>(frame.Rsp), 8)) {
                    break;
                }
                frame.Rip = *reinterpret_cast<DWORD64*>(frame.Rsp);
                frame.Rsp += 8;
                continue;
            }

            void* handlerData = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, frame.Rip, function, &frame, &handlerData, &establisher, nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LogLine("  (stack unwind faulted)\n");
            break;
        }
    }
#endif
}

static LONG CALLBACK FirstChanceHandler(EXCEPTION_POINTERS* info) {
    static volatile LONG s_inHandler = 0;
    DWORD code = info->ExceptionRecord->ExceptionCode;

    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_STACK_OVERFLOW || code == EXCEPTION_PRIV_INSTRUCTION) {
        if (InterlockedCompareExchange(&s_inHandler, 1, 0) != 0) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        char where[MAX_PATH + 32];
        ModuleOffset(info->ExceptionRecord->ExceptionAddress, where, sizeof(where));

        char line[1024];

        if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
            _snprintf_s(line, sizeof(line), _TRUNCATE, "CRASH 0x%08lX at %s (%s 0x%llX)\n", code, where, info->ExceptionRecord->ExceptionInformation[0] ? "write" : "read", static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[1]));
        } else {
            _snprintf_s(line, sizeof(line), _TRUNCATE, "CRASH 0x%08lX at %s\n", code, where);
        }

        LogLine(line);

        // Also write a minidump FIRST before symbol resolution
        char dumpPath[MAX_PATH];
        strcpy_s(dumpPath, sizeof(dumpPath), s_logPath);
        char* ext = strrchr(dumpPath, '.');
        if (ext) {
            strcpy_s(ext, sizeof(dumpPath) - (ext - dumpPath), ".dmp");
        }
        HANDLE hDump = CreateFileA(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hDump != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = info;
            mei.ClientPointers = FALSE;
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDump, MiniDumpNormal, &mei, nullptr, nullptr);
            CloseHandle(hDump);
        }

        __try {
            LogStack(info->ContextRecord);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LogLine("  (LogStack faulted)\n");
        }

        s_inHandler = 0;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static int CrtReportHook(int type, char* message, int* returnValue) {
    char line[4096];
    _snprintf_s(line, sizeof(line), _TRUNCATE, "CRTREPORT %s: %s%s", type == _CRT_ASSERT ? "assert" : type == _CRT_ERROR ? "error" : "warning", message ? message : "", message && strchr(message, '\n') ? "" : "\n");
    LogLine(line);

    if (type != _CRT_WARN) {
        CONTEXT context;
        RtlCaptureContext(&context);
        LogStack(&context);
        ExitProcess(0x5718);
    }

    *returnValue = 0;
    return TRUE;
}

static struct CrashLogInit {
    CrashLogInit() {
        GetModuleFileNameA(nullptr, s_logPath, sizeof(s_logPath));
        char* slash = strrchr(s_logPath, '\\');

        if (slash) {
            strcpy_s(slash + 1, sizeof(s_logPath) - (slash + 1 - s_logPath), "WowClient_crash.log");
        }

        DeleteFileA(s_logPath);
        s_logFile = CreateFileA(s_logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (AllocConsole()) {
            SetConsoleTitleA("WowClient");
            s_console = CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        }

        AddVectoredExceptionHandler(1, FirstChanceHandler);
        _CrtSetReportHook(CrtReportHook);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        atexit(OnProcessExit);
        g_pfnCrashLogWriteStack = CrashLogWriteStack;
        LogLine("=== WOWCLIENT CRASH LOGGER INITIALIZED ===\n");
    }
} s_crashLogInit;
