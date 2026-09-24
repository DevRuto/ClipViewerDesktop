#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>

namespace cv {

// Thrown by long-running work that noticed its CancelToken was cancelled.
class OperationCancelled : public std::runtime_error
{
public:
    OperationCancelled() : std::runtime_error("The operation was cancelled") {}
};

// ffmpeg or ffprobe failed; what() holds its error output.
class FfmpegError : public std::runtime_error
{
public:
    FfmpegError(const QString &message, int exitCode)
        : std::runtime_error(message.toStdString()), m_exitCode(exitCode) {}
    int exitCode() const { return m_exitCode; }

private:
    int m_exitCode;
};

// The file isn't what we expected (e.g. no video stream).
class MediaError : public std::runtime_error
{
public:
    explicit MediaError(const QString &message) : std::runtime_error(message.toStdString()) {}
};

// Cooperative cancellation, shared by copy. A linked token is cancelled when either it or its
// parent is, so a failing job can stop its siblings without cancelling the caller.
class CancelToken
{
public:
    CancelToken() : m_flag(std::make_shared<std::atomic_bool>(false)) {}

    void cancel() const { m_flag->store(true); }
    bool isCancelled() const { return m_flag->load() || (m_parent && m_parent->load()); }
    void throwIfCancelled() const
    {
        if (isCancelled())
            throw OperationCancelled();
    }
    CancelToken linked() const
    {
        CancelToken child;
        child.m_parent = m_flag;
        return child;
    }

private:
    std::shared_ptr<std::atomic_bool> m_flag;
    std::shared_ptr<std::atomic_bool> m_parent;
};

struct ProcessResult
{
    int exitCode = 0;
    QByteArray standardOutput;
    QByteArray standardError;
};

// Runs a process to completion, blocking the calling thread (call it from a worker thread). Kills
// the process and throws OperationCancelled when the token is cancelled. onLine gets each stdout
// line as it arrives; the output is then not kept in the result. Throws FfmpegError if the
// program can't be started; a non-zero exit code is returned, not thrown.
ProcessResult runProcess(const QString &program, const QStringList &args, const CancelToken &cancel,
                         const std::function<void(const QString &)> &onLine = {});

// runProcess, but a non-zero exit throws FfmpegError with the tool's stderr. Returns stdout.
QByteArray runTool(const QString &program, const QStringList &args, const CancelToken &cancel,
                   const std::function<void(const QString &)> &onLine = {});

} // namespace cv
