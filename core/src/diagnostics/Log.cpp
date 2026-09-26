#include "Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QThread>

#include <atomic>
#include <mutex>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace cv::Log {

namespace {

std::atomic<qintptr> g_handle{-1};
std::mutex g_mutex; // keeps lines whole and in order; the crash handler doesn't take it
qint64 g_written = 0;
bool g_truncated = false;
QString g_path;
QtMessageHandler g_previous = nullptr;
std::atomic<FatalHook> g_fatalHook{nullptr};

const QString CurrentName = QStringLiteral("app.log");

QString oldName(int run)
{
    return QStringLiteral("app.%1.log").arg(run);
}

qintptr openAppend(const QString &path)
{
#ifdef Q_OS_WIN
    // Shared for delete, so another running copy can still rotate it away.
    const HANDLE file = CreateFileW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(path).utf16()),
                                    FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    return file == INVALID_HANDLE_VALUE ? -1 : reinterpret_cast<qintptr>(file);
#else
    return ::open(QFile::encodeName(path).constData(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
#endif
}

void closeHandle(qintptr handle)
{
#ifdef Q_OS_WIN
    CloseHandle(reinterpret_cast<HANDLE>(handle));
#else
    ::close(static_cast<int>(handle));
#endif
}

// Under g_mutex.
void writeLine(const QByteArray &line, bool force)
{
    if (g_written + line.size() > MaxBytes && !force) {
        if (!g_truncated) {
            g_truncated = true;
            static const char note[] = "--- log full, later messages are left out ---\n";
            writeRaw(note, sizeof(note) - 1);
        }
        return;
    }
    writeRaw(line.constData(), static_cast<std::size_t>(line.size()));
    g_written += line.size();
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (g_previous)
        g_previous(type, context, message);
    try {
        const QByteArray line = formatLine(type, context.category, message, QDateTime::currentDateTime(),
                                           reinterpret_cast<quintptr>(QThread::currentThreadId()));
        std::lock_guard lock(g_mutex);
        writeLine(line, type == QtFatalMsg);
    } catch (...) {
        // Logging must never take the app down.
    }
    if (type == QtFatalMsg)
        if (const FatalHook hook = g_fatalHook.load())
            hook(qUtf8Printable(message));
}

} // namespace

QString defaultDir()
{
    // GenericDataLocation is %LOCALAPPDATA% on Windows.
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("ClipViewerDesktop/logs"));
}

void rotate(const QString &dir, int keep)
{
    QDir folder(dir);
    if (keep <= 0) {
        folder.remove(CurrentName);
        return;
    }
    folder.remove(oldName(keep));
    for (int run = keep - 1; run >= 1; --run)
        folder.rename(oldName(run), oldName(run + 1));
    folder.rename(CurrentName, oldName(1));
}

bool start(const QString &dir, const QString &header)
{
    try {
        if (g_handle.load() != -1)
            return true;
        if (!QDir().mkpath(dir))
            return false;
        rotate(dir);
        const QString path = QDir(dir).filePath(CurrentName);
        const qintptr handle = openAppend(path);
        if (handle == -1)
            return false;

        std::lock_guard lock(g_mutex);
        g_handle = handle;
        g_path = path;
        g_written = 0;
        g_truncated = false;
        QByteArray text = header.toUtf8();
        if (!text.endsWith('\n'))
            text += '\n';
        writeLine(text + '\n', false);
        g_previous = qInstallMessageHandler(messageHandler);
        return true;
    } catch (...) {
        return false;
    }
}

void stop(const QString &lastLine)
{
    if (g_handle.load() == -1)
        return;
    if (!lastLine.isEmpty())
        qInfo("%s", qUtf8Printable(lastLine));
    qInstallMessageHandler(g_previous);
    std::lock_guard lock(g_mutex);
    closeHandle(g_handle.exchange(-1));
    g_path.clear();
    g_previous = nullptr;
}

QString currentPath()
{
    std::lock_guard lock(g_mutex);
    return g_path;
}

QString previousPath(const QString &dir)
{
    return QDir(dir).filePath(oldName(1));
}

bool hasCrashReport(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    // The marker always starts a line after the header.
    return file.readAll().contains(QByteArray("\n") + CrashMarker);
}

QByteArray formatLine(QtMsgType type, const char *category, const QString &message, const QDateTime &time,
                      quintptr threadId)
{
    char level = 'D';
    switch (type) {
    case QtDebugMsg: level = 'D'; break;
    case QtInfoMsg: level = 'I'; break;
    case QtWarningMsg: level = 'W'; break;
    case QtCriticalMsg: level = 'C'; break;
    case QtFatalMsg: level = 'F'; break;
    }

    QByteArray line = time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")).toLatin1();
    line += ' ';
    line += level;
    line += " [";
    line += QByteArray::number(static_cast<qulonglong>(threadId), 16);
    line += "] ";
    if (category && qstrcmp(category, "default") != 0) {
        line += category;
        line += ": ";
    }
    QByteArray text = message.toUtf8();
    while (text.endsWith('\n') || text.endsWith('\r'))
        text.chop(1);
    text.replace("\r\n", "\n");
    text.replace("\n", "\n    ");
    line += text;
    line += '\n';
    return line;
}

void writeRaw(const char *data, std::size_t size)
{
    const qintptr handle = g_handle.load();
    if (handle == -1 || size == 0)
        return;
#ifdef Q_OS_WIN
    DWORD written = 0;
    WriteFile(reinterpret_cast<HANDLE>(handle), data, static_cast<DWORD>(size), &written, nullptr);
#else
    while (size > 0) {
        const ssize_t written = ::write(static_cast<int>(handle), data, size);
        if (written <= 0)
            return;
        data += written;
        size -= static_cast<std::size_t>(written);
    }
#endif
}

void setFatalHook(FatalHook hook)
{
    g_fatalHook = hook;
}

qintptr nativeHandle()
{
    return g_handle.load();
}

} // namespace cv::Log
