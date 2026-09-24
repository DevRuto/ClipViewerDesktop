#pragma once

#include <QJsonObject>
#include <QString>

namespace cv {

// User preferences, stored as camelCase JSON in the same file and format as the .NET app, so both
// builds share one settings.json. Keys this build doesn't know are kept when saving.
struct AppSettings
{
    // Use the compact, video-first layout instead of the one with the side panel.
    bool compactLayout = false;
    // The ClipViewer server's base address; empty until the user signs in. The API key is kept in
    // the OS credential store, never here.
    QString serverUrl;
    // The signed-in account's name, shown without asking the server.
    QString serverUsername;
    // The export button's main action uploads to ClipViewer instead of saving a file.
    bool uploadByDefault = false;
    // The clip player hides its details sidebar.
    bool hideClipDetails = false;

    // %LOCALAPPDATA%\ClipViewerDesktop\settings.json on Windows.
    static QString defaultPath();

    // Reads the settings, or returns the defaults if the file is missing or unreadable.
    static AppSettings load(const QString &path);

    // Writes the settings, replacing the file only once the new one is complete. Returns false if
    // it couldn't be written.
    bool save(const QString &path) const;

    bool operator==(const AppSettings &other) const;

private:
    QJsonObject m_unknown; // fields from the file this build doesn't use
};

} // namespace cv
