#include "AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace cv {

namespace {

constexpr auto CompactLayout = "compactLayout";
constexpr auto ServerUrl = "serverUrl";
constexpr auto ServerUsername = "serverUsername";
constexpr auto UploadByDefault = "uploadByDefault";
constexpr auto HideClipDetails = "hideClipDetails";

} // namespace

QString AppSettings::defaultPath()
{
    // GenericDataLocation is %LOCALAPPDATA% on Windows; the folder name matches the .NET app.
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
    settings.compactLayout = json.take(QLatin1String(CompactLayout)).toBool();
    settings.serverUrl = json.take(QLatin1String(ServerUrl)).toString();
    settings.serverUsername = json.take(QLatin1String(ServerUsername)).toString();
    settings.uploadByDefault = json.take(QLatin1String(UploadByDefault)).toBool();
    settings.hideClipDetails = json.take(QLatin1String(HideClipDetails)).toBool();
    settings.m_unknown = json;
    return settings;
}

bool AppSettings::save(const QString &path) const
{
    QJsonObject json = m_unknown;
    json[QLatin1String(CompactLayout)] = compactLayout;
    json[QLatin1String(ServerUrl)] = serverUrl.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(serverUrl);
    json[QLatin1String(ServerUsername)] =
        serverUsername.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(serverUsername);
    json[QLatin1String(UploadByDefault)] = uploadByDefault;
    json[QLatin1String(HideClipDetails)] = hideClipDetails;

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool AppSettings::operator==(const AppSettings &other) const
{
    return compactLayout == other.compactLayout && serverUrl == other.serverUrl
        && serverUsername == other.serverUsername && uploadByDefault == other.uploadByDefault
        && hideClipDetails == other.hideClipDetails && m_unknown == other.m_unknown;
}

} // namespace cv
