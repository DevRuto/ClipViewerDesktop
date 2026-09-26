#include "CrashHandler.h"

#include "Log.h"

#include <QDir>
#include <QFileInfo>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>

#include <cxxabi.h>

#ifdef Q_OS_WIN
#include <windows.h>
// After windows.h
#include <dbghelp.h>
#else
#include <csignal>
#include <ctime>
#include <execinfo.h>
#include <unistd.h>
#endif

namespace cv::CrashHandler {

namespace {

std::atomic<bool> g_crashing{false};
constexpr int KeptDumps = 3;
constexpr int MaxFrames = 64;

// Formats into a stack buffer and appends to the log: no allocation.
void out(const char *format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length > 0)
        Log::writeRaw(buffer, std::min<std::size_t>(static_cast<std::size_t>(length), sizeof(buffer) - 1));
}

// The message of the exception being handled in a terminate handler, into `buffer`.
void currentExceptionMessage(char *buffer, std::size_t size)
{
    std::snprintf(buffer, size, "no exception");
    if (const std::exception_ptr current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception &e) {
            std::snprintf(buffer, size, "%s", e.what());
        } catch (...) {
            std::snprintf(buffer, size, "not a std::exception");
        }
    }
}

#ifdef Q_OS_WIN

wchar_t g_dumpDir[MAX_PATH];     // ends in a backslash
wchar_t g_symbolPath[MAX_PATH];  // the exe's folder; keeps DbgHelp off _NT_SYMBOL_PATH and servers
DWORD g_writerThread = 0;

struct CrashInfo
{
    const char *reason;
    EXCEPTION_POINTERS *exception; // null for abort() and terminate
    CONTEXT context;
    DWORD threadId;
};

const char *exceptionName(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned data";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "noncontinuable exception";
    case 0x40000015: return "fatal app exit (qFatal)";
    case 0xC0000409: return "stack buffer overrun";
    case 0xC0000374: return "heap corruption";
    case 0x20474343: return "uncaught C++ exception";
    default: return "exception";
    }
}

// One stack frame: address, module+offset and, when DbgHelp finds one, the function.
void describeFrame(int index, DWORD64 address, bool symbols)
{
    HMODULE module = nullptr;
    char moduleName[MAX_PATH] = "?";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(address), &module)) {
        char path[MAX_PATH];
        if (GetModuleFileNameA(module, path, MAX_PATH)) {
            const char *slash = std::strrchr(path, '\\');
            std::snprintf(moduleName, sizeof(moduleName), "%s", slash ? slash + 1 : path);
        }
    }
    const auto offset = static_cast<unsigned long long>(address - reinterpret_cast<DWORD64>(module));

    char function[512] = "";
    if (symbols) {
        alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256];
        auto *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 256;
        DWORD64 displacement = 0;
        // Only exported names are known (DbgHelp can't read MinGW's DWARF), so a frame far from
        // its nearest export is in some other, unexported function: leave it unnamed.
        if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbol) && displacement < 0x2000) {
            // DbgHelp strips the leading underscore off GCC's mangled "_Z…" names.
            char mangled[260];
            std::snprintf(mangled, sizeof(mangled), "_%s", symbol->Name);
            int status = -1;
            char *demangled = symbol->Name[0] == 'Z' ? abi::__cxa_demangle(mangled, nullptr, nullptr, &status)
                                                     : nullptr;
            std::snprintf(function, sizeof(function), "%s+0x%llx", status == 0 ? demangled : symbol->Name,
                          static_cast<unsigned long long>(displacement));
            std::free(demangled);
        }
    }
    out("  #%-2d %s+0x%llx  %s\n", index, moduleName, module ? offset : static_cast<unsigned long long>(address),
        function);
}

void writeStack(CONTEXT context, bool symbols)
{
#if defined(__x86_64__) || defined(_M_X64)
    // Unwinds with the x64 unwind tables (.pdata), which MinGW emits too, so no PDBs are needed.
    for (int frame = 0; frame < MaxFrames && context.Rip != 0; ++frame) {
        describeFrame(frame, context.Rip, symbols);
        DWORD64 imageBase = 0;
        if (PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr)) {
            void *handlerData = nullptr;
            DWORD64 establisherFrame = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData,
                             &establisherFrame, nullptr);
        } else {
            // A leaf function: the return address is on top of the stack. Read it safely.
            DWORD64 returnAddress = 0;
            SIZE_T read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(context.Rsp), &returnAddress,
                                   sizeof(returnAddress), &read)
                || read != sizeof(returnAddress))
                break;
            context.Rip = returnAddress;
            context.Rsp += sizeof(returnAddress);
        }
    }
#else
    (void)context;
    (void)symbols;
    out("  (no stack trace on this architecture)\n");
#endif
}

void writeMinidump(CrashInfo &info, const SYSTEMTIME &time)
{
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%lscrash-%04d%02d%02d-%02d%02d%02d.dmp", g_dumpDir, time.wYear, time.wMonth,
               time.wDay, time.wHour, time.wMinute, time.wSecond);
    path[MAX_PATH - 1] = 0;
    const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    MINIDUMP_EXCEPTION_INFORMATION exception{info.threadId, info.exception, FALSE};
    const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
    const bool written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type,
                                           info.exception ? &exception : nullptr, nullptr, nullptr);
    CloseHandle(file);
    if (written) {
        char narrow[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, MAX_PATH, nullptr, nullptr);
        out("Minidump: %s\n", narrow);
    } else {
        DeleteFileW(path);
    }
}

// Runs on its own thread: the crashed one may hold the heap lock or have no stack left.
DWORD WINAPI writeReport(void *parameter)
{
    auto &info = *static_cast<CrashInfo *>(parameter);
    SYSTEMTIME time;
    GetLocalTime(&time);
    out("\n%s %04d-%02d-%02d %02d:%02d:%02d ===\n", Log::CrashMarker, time.wYear, time.wMonth, time.wDay,
        time.wHour, time.wMinute, time.wSecond);
    out("%s\n", info.reason);
    if (info.exception) {
        const EXCEPTION_RECORD &record = *info.exception->ExceptionRecord;
        out("Exception 0x%08lx (%s) at 0x%llx\n", record.ExceptionCode, exceptionName(record.ExceptionCode),
            reinterpret_cast<unsigned long long>(record.ExceptionAddress));
        if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
            const ULONG_PTR operation = record.ExceptionInformation[0];
            out("  %s address 0x%llx\n", operation == 0 ? "reading" : operation == 1 ? "writing" : "executing",
                static_cast<unsigned long long>(record.ExceptionInformation[1]));
        }
    }
    out("Thread 0x%lx, stack:\n", info.threadId);

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    const bool symbols = SymInitializeW(GetCurrentProcess(), g_symbolPath, TRUE);
    writeStack(info.context, symbols);
    writeMinidump(info, time);
    out("===\n");
    return 0;
}

void report(const char *reason, const CONTEXT &context, EXCEPTION_POINTERS *exception)
{
    if (g_crashing.exchange(true)) {
        if (GetCurrentThreadId() != g_writerThread)
            Sleep(30000); // another thread is writing its report; the process ends after that
        return;
    }
    static CrashInfo info; // not on the crashed thread's stack, which may be nearly used up
    info.reason = reason;
    info.exception = exception;
    info.context = context;
    info.threadId = GetCurrentThreadId();
    if (const HANDLE writer = CreateThread(nullptr, 512 * 1024, writeReport, &info, 0, &g_writerThread)) {
        // A report that hangs (a lock held by the crashed thread) mustn't leave a frozen window.
        WaitForSingleObject(writer, 20000);
        CloseHandle(writer);
    } else {
        writeReport(&info);
    }
}

LONG WINAPI onUnhandledException(EXCEPTION_POINTERS *exception)
{
    report("The app crashed.", *exception->ContextRecord, exception);
    return EXCEPTION_EXECUTE_HANDLER; // ends the process without the Windows error dialog
}

void onAbort(int)
{
    CONTEXT context;
    RtlCaptureContext(&context);
    report("abort() was called.", context, nullptr);
    TerminateProcess(GetCurrentProcess(), 3);
}

void onTerminate()
{
    static char reason[600];
    char message[512];
    currentExceptionMessage(message, sizeof(message));
    std::snprintf(reason, sizeof(reason), "Uncaught C++ exception: %s", message);
    CONTEXT context;
    RtlCaptureContext(&context);
    report(reason, context, nullptr);
    TerminateProcess(GetCurrentProcess(), 3);
}

// Qt ends a qFatal with a fail-fast exception, which skips every handler: report it first.
void onFatal(const char *message)
{
    static char reason[600];
    std::snprintf(reason, sizeof(reason), "qFatal: %s", message);
    CONTEXT context;
    RtlCaptureContext(&context);
    report(reason, context, nullptr);
    TerminateProcess(GetCurrentProcess(), 3);
}

void removeOldDumps(const QString &dir)
{
    const QFileInfoList dumps = QDir(dir).entryInfoList({QStringLiteral("crash-*.dmp")}, QDir::Files, QDir::Time);
    for (qsizetype i = KeptDumps - 1; i < dumps.size(); ++i)
        QFile::remove(dumps[i].absoluteFilePath());
}

#else // POSIX

void onSignal(int signal)
{
    if (!g_crashing.exchange(true)) {
        char stamp[32] = "";
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        if (localtime_r(&now, &local))
            std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
        out("\n%s %s ===\nThe app crashed: signal %d (%s)\nStack:\n", Log::CrashMarker, stamp, signal,
            strsignal(signal));
        void *frames[MaxFrames];
        const int count = backtrace(frames, MaxFrames);
        if (Log::nativeHandle() != -1)
            backtrace_symbols_fd(frames, count, static_cast<int>(Log::nativeHandle()));
        out("===\n");
    }
    // SA_RESETHAND restored the default action: crash for real (core dump, OS crash reporter).
    raise(signal);
}

void onTerminate()
{
    char message[512];
    currentExceptionMessage(message, sizeof(message));
    out("Uncaught C++ exception: %s\n", message);
    std::abort(); // SIGABRT writes the report
}

#endif

} // namespace

void install(const QString &dumpDir)
{
    std::set_terminate(onTerminate);
#ifdef Q_OS_WIN
    const QString dir = QDir::toNativeSeparators(QDir(dumpDir).absolutePath()) + QLatin1Char('\\');
    wcsncpy(g_dumpDir, reinterpret_cast<const wchar_t *>(dir.utf16()), MAX_PATH - 1);
    GetModuleFileNameW(nullptr, g_symbolPath, MAX_PATH);
    if (wchar_t *slash = wcsrchr(g_symbolPath, L'\\'))
        *slash = 0;
    removeOldDumps(dumpDir);

    SetUnhandledExceptionFilter(onUnhandledException);
    Log::setFatalHook(onFatal);
    // onAbort ends the process itself, before the runtime's abort message box.
    signal(SIGABRT, onAbort);
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    // Leaves room on the main thread's stack to report a stack overflow.
    ULONG guarantee = 64 * 1024;
    SetThreadStackGuarantee(&guarantee);
#else
    (void)dumpDir;
    // A separate stack for the handler, so a stack overflow can be reported.
    static char alternateStack[64 * 1024];
    stack_t stack{};
    stack.ss_sp = alternateStack;
    stack.ss_size = sizeof(alternateStack);
    sigaltstack(&stack, nullptr);
    // backtrace() loads libgcc on first use, which isn't safe in a signal handler.
    void *frames[1];
    backtrace(frames, 1);

    struct sigaction action{};
    action.sa_handler = onSignal;
    action.sa_flags = SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    for (int signal : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
        sigaction(signal, &action, nullptr);
#endif
}

void crashForTesting(const QString &kind)
{
    if (kind == QLatin1String("abort"))
        std::abort();
    if (kind == QLatin1String("throw"))
        throw std::runtime_error("crash test");
    if (kind == QLatin1String("fatal"))
        qFatal("crash test");
    volatile int *null = nullptr;
    *null = 1;
}

} // namespace cv::CrashHandler
