#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

// Serves the ffmpeg-decoded still frame to QML as image://still/<generation>. Only the latest
// frame is kept; the generation in the URL just makes QML reload it.
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
