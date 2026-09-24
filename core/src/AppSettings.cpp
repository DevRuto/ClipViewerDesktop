#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace cv {

namespace {

const QString Volume = QStringLiteral("volume");
const QString Muted = QStringLiteral("muted");
const QString SmartCut = QStringLiteral("smartCut");
const QString LastExportFolder = QStringLiteral("lastExportFolder");

} // namespace

QString AppSettings::defaultPath()
{
    // GenericDataLocation is %LOCALAPPDATA% on Windows.
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("ClipViewerDesktop/settings.json"));
}

AppSettings AppSettings::load(const QString &path)
{
    AppSettings settings;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return settings;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return settings;

    QJsonObject json = doc.object();
    settings.volume = std::clamp(json.take(Volume).toDouble(settings.volume), 0.0, 1.0);
    settings.muted = json.take(Muted).toBool(settings.muted);
    settings.smartCut = json.take(SmartCut).toBool(settings.smartCut);
    settings.lastExportFolder = json.take(LastExportFolder).toString();
    settings.m_unknown = json;
    return settings;
}

bool AppSettings::save(const QString &path) const
{
    QJsonObject json = m_unknown;
    json[Volume] = volume;
    json[Muted] = muted;
    json[SmartCut] = smartCut;
    json[LastExportFolder] = lastExportFolder;

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool AppSettings::operator==(const AppSettings &other) const
{
    return volume == other.volume && muted == other.muted && smartCut == other.smartCut
        && lastExportFolder == other.lastExportFolder && m_unknown == other.m_unknown;
}

} // namespace cv
