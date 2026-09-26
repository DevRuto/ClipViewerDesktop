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
const QString Reencode = QStringLiteral("reencode");
const QString LastExportFolder = QStringLiteral("lastExportFolder");
const QString LastFrameFolder = QStringLiteral("lastFrameFolder");
const QString Theme = QStringLiteral("theme");
const QString Style = QStringLiteral("style");
const QString Font = QStringLiteral("font");
const QString Compact = QStringLiteral("compact");
const QString AlwaysOnTop = QStringLiteral("alwaysOnTop");
const QString ClickControls = QStringLiteral("clickControls");
const QString EdgeDoubleClick = QStringLiteral("edgeDoubleClick");
const QString ShortSkip = QStringLiteral("shortSkip");
const QString LongSkip = QStringLiteral("longSkip");
const QString Autoplay = QStringLiteral("autoplay");
const QString Subtitles = QStringLiteral("subtitles");

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
    settings.reencode = ReencodeOptions::fromJson(json.take(Reencode).toObject());
    settings.lastExportFolder = json.take(LastExportFolder).toString();
    settings.lastFrameFolder = json.take(LastFrameFolder).toString();
    settings.theme = json.take(Theme).toString(settings.theme);
    settings.style = json.take(Style).toString(settings.style);
    settings.font = json.take(Font).toString(settings.font);
    settings.compact = json.take(Compact).toBool(settings.compact);
    settings.alwaysOnTop = json.take(AlwaysOnTop).toBool(settings.alwaysOnTop);
    settings.clickControls = json.take(ClickControls).toBool(settings.clickControls);
    settings.edgeDoubleClick = json.take(EdgeDoubleClick).toBool(settings.edgeDoubleClick);
    settings.shortSkip = std::clamp(json.take(ShortSkip).toInt(settings.shortSkip), 1, 60);
    settings.longSkip = std::clamp(json.take(LongSkip).toInt(settings.longSkip), 1, 600);
    settings.autoplay = json.take(Autoplay).toBool(settings.autoplay);
    settings.subtitles = SubtitleStyle::fromJson(json.take(Subtitles).toObject());
    settings.m_unknown = json;
    return settings;
}

bool AppSettings::save(const QString &path) const
{
    QJsonObject json = m_unknown;
    json[Volume] = volume;
    json[Muted] = muted;
    json[SmartCut] = smartCut;
    json[Reencode] = reencode.toJson();
    json[LastExportFolder] = lastExportFolder;
    json[LastFrameFolder] = lastFrameFolder;
    json[Theme] = theme;
    json[Style] = style;
    json[Font] = font;
    json[Compact] = compact;
    json[AlwaysOnTop] = alwaysOnTop;
    json[ClickControls] = clickControls;
    json[EdgeDoubleClick] = edgeDoubleClick;
    json[ShortSkip] = shortSkip;
    json[LongSkip] = longSkip;
    json[Autoplay] = autoplay;
    json[Subtitles] = subtitles.toJson();

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
        && reencode == other.reencode && lastExportFolder == other.lastExportFolder
        && lastFrameFolder == other.lastFrameFolder && theme == other.theme
        && style == other.style && font == other.font && compact == other.compact
        && alwaysOnTop == other.alwaysOnTop && clickControls == other.clickControls
        && edgeDoubleClick == other.edgeDoubleClick && shortSkip == other.shortSkip
        && longSkip == other.longSkip && autoplay == other.autoplay && subtitles == other.subtitles
        && m_unknown == other.m_unknown;
}

} // namespace cv
