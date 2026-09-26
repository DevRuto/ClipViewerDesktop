#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QPointer>
#include <QThreadPool>

// Runs work() on the thread pool and done(result) back on the UI thread, unless `owner` is gone
// by then. work must not throw.
template <typename Work, typename Done>
void runInBackground(QObject *owner, Work work, Done done)
{
    QPointer<QObject> guard(owner);
    QThreadPool::globalInstance()->start([guard, work = std::move(work), done = std::move(done)]() mutable {
        auto result = work();
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [guard, done = std::move(done), result = std::move(result)]() mutable {
                if (guard)
                    done(std::move(result));
            },
            Qt::QueuedConnection);
    });
}

// The first line of an error message; ffmpeg's can run to many.
inline QString firstLine(const char *message)
{
    return QString::fromUtf8(message).section(QChar('\n'), 0, 0).trimmed();
}
