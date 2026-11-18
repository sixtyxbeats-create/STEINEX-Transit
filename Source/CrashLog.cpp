#include <juce_core/juce_core.h>
#include "Log.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <cstdio>
 #include <cstring>
 #include <string>

namespace
{
std::string gLogPath;
volatile LONG gRefs = 0;
volatile LONG gReports = 0;
PVOID gHandler = nullptr;
char gBuf[4096];

void appendRaw (const char* text, size_t n)
{
    HANDLE h = CreateFileA (gLogPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile (h, text, (DWORD) n, &written, nullptr);
    CloseHandle (h);
}

int describe (char* out, size_t cap, void* addr)
{
    HMODULE mod = nullptr;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR) addr, &mod) && mod != nullptr)
    {
        GetModuleFileNameA (mod, name, MAX_PATH);
        const char* slash = std::strrchr (name, '\\');
        return std::snprintf (out, cap, "%s+0x%llx", slash ? slash + 1 : name,
                              (unsigned long long) ((char*) addr - (char*) mod));
    }
    return std::snprintf (out, cap, "unknown 0x%llx", (unsigned long long) addr);
}

LONG CALLBACK handler (EXCEPTION_POINTERS* ep)
{
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    const bool fatal = code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_STACK_OVERFLOW
                    || code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_INT_DIVIDE_BY_ZERO
                    || code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED || code == EXCEPTION_PRIV_INSTRUCTION
                    || code == 0xC0000374 || code == 0xC0000409;
    if (! fatal || InterlockedIncrement (&gReports) > 3) return EXCEPTION_CONTINUE_SEARCH;

    int n = std::snprintf (gBuf, sizeof (gBuf), "!!! CRASH code=0x%08lx thread=%lu  at ", (unsigned long) code, (unsigned long) GetCurrentThreadId());
    n += describe (gBuf + n, sizeof (gBuf) - (size_t) n, ep->ExceptionRecord->ExceptionAddress);
    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
        n += std::snprintf (gBuf + n, sizeof (gBuf) - (size_t) n, "  (%s address 0x%llx)",
                            ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "read" : (ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "write" : "exec"),
                            (unsigned long long) ep->ExceptionRecord->ExceptionInformation[1]);
    n += std::snprintf (gBuf + n, sizeof (gBuf) - (size_t) n, "\r\n    stack:");
    void* frames[20];
    const USHORT cnt = CaptureStackBackTrace (0, 20, frames, nullptr);
    for (USHORT i = 0; i < cnt && (size_t) n < sizeof (gBuf) - 200; ++i)
    {
        n += std::snprintf (gBuf + n, sizeof (gBuf) - (size_t) n, "\r\n      ");
        n += describe (gBuf + n, sizeof (gBuf) - (size_t) n, frames[i]);
    }
    n += std::snprintf (gBuf + n, sizeof (gBuf) - (size_t) n, "\r\n");
    appendRaw (gBuf, (size_t) n);
    return EXCEPTION_CONTINUE_SEARCH;
}
} // namespace

namespace steinex
{
void installCrashLogger()
{
    if (InterlockedIncrement (&gRefs) != 1) return;
    gLogPath = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                  .getChildFile ("STEINEX").getChildFile ("TRANSIT FX").getChildFile ("log.txt")
                  .getFullPathName().toStdString();
    InterlockedExchange (&gReports, 0);
    gHandler = AddVectoredExceptionHandler (1, handler);
}

void removeCrashLogger()
{
    if (InterlockedDecrement (&gRefs) != 0) return;
    if (gHandler != nullptr) { RemoveVectoredExceptionHandler (gHandler); gHandler = nullptr; }
}
}
#else
namespace steinex
{
void installCrashLogger() {}
void removeCrashLogger() {}
}
#endif
