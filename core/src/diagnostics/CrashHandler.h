#pragma once

#include <QString>

// Writes a crash report into the log (Log.h) when the app crashes, aborts (qFatal, a failed
// assert) or leaves a C++ exception uncaught: what happened and the crashing thread's stack, one
// frame per line as `module+offset  function`. On Windows a minidump (crash-<time>.dmp) goes
// next to the log as well.
//
// Only exported functions (Qt's, Windows') are named. For the app's own frames, run
// `addr2line -f -C -e ClipViewerDesktop.exe 0x<0x140000000 + offset>` on a build of the same
// commit with debug info (-g doesn't change GCC's code, so a Release crash resolves against a
// Release build with -g added).
namespace cv::CrashHandler {

// Call once at startup, after Log::start. Keeps the newest few minidumps in `dumpDir`.
void install(const QString &dumpDir);

// Crashes on purpose, for checking the handler: "segv", "abort", "throw" or "fatal".
void crashForTesting(const QString &kind);

} // namespace cv::CrashHandler
