#include "MainWindow.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Rhizomatica"));
    QCoreApplication::setApplicationName(QStringLiteral("MercuryChat"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Mercury Chat"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption profileOption(
        {QStringLiteral("P"), QStringLiteral("profile")},
        QStringLiteral("Use a named UI profile for window labeling and default settings-file selection."),
        QStringLiteral("name"));
    const QCommandLineOption settingsFileOption(
        {QStringLiteral("s"), QStringLiteral("settings-file")},
        QStringLiteral("Use this INI settings file instead of the platform QSettings store."),
        QStringLiteral("file"));
    parser.addOption(profileOption);
    parser.addOption(settingsFileOption);
    parser.process(app);

    const QString profileName = parser.value(profileOption).trimmed();
    QString settingsFile = parser.value(settingsFileOption).trimmed();
    if (settingsFile.isEmpty() && !profileName.isEmpty())
    {
        settingsFile = QDir(QCoreApplication::applicationDirPath())
                           .filePath(QStringLiteral("../profiles/%1.ini").arg(profileName.toLower()));
    }
    if (!settingsFile.isEmpty())
    {
        QFileInfo settingsInfo(settingsFile);
        QDir().mkpath(settingsInfo.absolutePath());
        settingsFile = settingsInfo.absoluteFilePath();
    }

    MainWindow window(settingsFile, profileName);
    window.show();
    return app.exec();
}
