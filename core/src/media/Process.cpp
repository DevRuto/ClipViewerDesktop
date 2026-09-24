#include "Process.h"

#include <QFileInfo>
#include <QProcess>

namespace cv {

namespace {

constexpr int PollMs = 50;

QString toolName(const QString &program)
{
    return QFileInfo(program).completeBaseName();
}

} // namespace

ProcessResult runProcess(const QString &program, const QStringList &args, const CancelToken &cancel,
                         const std::function<void(const QString &)> &onLine)
{
    cancel.throwIfCancelled();

    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    process.start(QIODevice::ReadOnly);
    if (!process.waitForStarted(-1))
        throw FfmpegError(QStringLiteral("Couldn't start %1: %2").arg(toolName(program), process.errorString()), -1);

    ProcessResult result;
    QByteArray pending; // stdout not yet split into lines
    auto drain = [&](bool final) {
        const QByteArray out = process.readAllStandardOutput();
        result.standardError += process.readAllStandardError();
        if (!onLine) {
            result.standardOutput += out;
            return;
        }
        pending += out;
        qsizetype newline;
        while ((newline = pending.indexOf('\n')) >= 0) {
            onLine(QString::fromUtf8(pending.left(newline)).trimmed());
            pending.remove(0, newline + 1);
        }
        if (final && !pending.isEmpty())
            onLine(QString::fromUtf8(pending).trimmed());
    };

    // QProcess buffers the pipes while waitFor* runs, so polling can't deadlock on a full pipe.
    while (process.state() != QProcess::NotRunning) {
        if (cancel.isCancelled()) {
            process.kill();
            process.waitForFinished(-1);
            throw OperationCancelled();
        }
        process.waitForFinished(PollMs);
        drain(false);
    }
    drain(true);
    // onLine may have cancelled after the process exited (a short run can finish within one poll).
    cancel.throwIfCancelled();

    result.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    return result;
}

QByteArray runTool(const QString &program, const QStringList &args, const CancelToken &cancel,
                   const std::function<void(const QString &)> &onLine)
{
    ProcessResult result = runProcess(program, args, cancel, onLine);
    if (result.exitCode != 0) {
        throw FfmpegError(QStringLiteral("%1 failed: %2")
                              .arg(toolName(program), QString::fromUtf8(result.standardError).trimmed()),
                          result.exitCode);
    }
    return result.standardOutput;
}

} // namespace cv
