#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

// Serves an ffmpeg-decoded frame to QML as image://<id>/<generation>: the still shown while paused
// ("still"), the timeline's hover preview ("thumb") and the DVD subtitle picture showing
// ("subtitle"). Only the latest frame is kept; the
// generation in the URL just makes QML reload it.
class StillFrameProvider : public QQuickImageProvider
{
public:
    StillFrameProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    void setFrame(const QImage &frame)
    {
        QMutexLocker lock(&m_mutex);
        m_frame = frame;
    }

    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        QMutexLocker lock(&m_mutex);
        if (size)
            *size = m_frame.size();
        return m_frame;
    }

private:
    QMutex m_mutex;
    QImage m_frame;
};
