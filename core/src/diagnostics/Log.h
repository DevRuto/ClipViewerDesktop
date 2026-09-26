#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <cstddef>

class QDateTime;

// The app's log file: every Qt message (ours, Qt's, QML warnings) goes to app.log as well as to
// the usual output, so users can attach it to a bug report. The last few runs are kept.
namespace cv::Log {

// %LOCALAPPDATA%\ClipViewerDesktop\logs on Windows.
QString defaultDir();

// Old logs kept next to app.log: app.1.log (the previous run) … app.<KeptRuns>.log.
inline constexpr int KeptRuns = 4;

// A run stops logging (apart from fatal messages) after this much, so a message loop can't fill
// the disk.
inline constexpr qint64 MaxBytes = 20 * 1024 * 1024;

// The crash handler starts its report with this line; the next run looks for it.
inline constexpr char CrashMarker[] = "=== CRASH";

// Moves app.log to app.1.log, app.1.log to app.2.log and so on, dropping the oldest past `keep`.
void rotate(const QString &dir, int keep = KeptRuns);

// Rotates the old logs, starts a new app.log in `dir` with `header` at the top, and installs the
// message handler. Returns false if the file can't be created; messages then only go to the
// usual output. Never throws.
bool start(const QString &dir, const QString &header);

// Writes a last line, removes the handler and closes the file.
void stop(const QString &lastLine = {});

// The file being written, or empty when not logging.
QString currentPath();

// The previous run's log (app.1.log once start() has rotated).
QString previousPath(const QString &dir);

// Whether the log at `path` holds a crash report.
bool hasCrashReport(const QString &path);

// "2026-09-26 14:03:12.345 W [1a2c] qt.multimedia: message\n". The category is left out for
// plain qDebug/qWarning calls ("default"); continuation lines are indented.
QByteArray formatLine(QtMsgType type, const char *category, const QString &message, const QDateTime &time,
                      quintptr threadId);

// Appends bytes straight to the log file, with no lock or allocation, for the crash handler. Does
// nothing when not logging.
void writeRaw(const char *data, std::size_t size);

// Called after a fatal message (qFatal) is written, before Qt ends the process.
using FatalHook = void (*)(const char *message);
void setFatalHook(FatalHook hook);

// The open file's native handle (a HANDLE on Windows, a file descriptor elsewhere), or -1.
qintptr nativeHandle();

} // namespace cv::Log
