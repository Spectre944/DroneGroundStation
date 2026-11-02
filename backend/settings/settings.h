#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QMutex>
#include <QFont>
#include <QFontDatabase>

enum FONT_SIZE {
    SMALL = 0,
    MEDIUM,
    BIG
};

class Settings : public QObject
{
    Q_OBJECT

public:
    // Единый метод доступа к экземпляру
    static Settings& instance(const QString& filePath = QString()) {
        static QMutex mutex;
        QMutexLocker locker(&mutex);

        static Settings* instancePtr = nullptr;
        if (!instancePtr) {
            instancePtr = new Settings(filePath);
        } else if (!filePath.isEmpty()) {
            instancePtr->setSettingsFilePath(filePath);
        }

        return *instancePtr;
    }

    // Запрещаем копирование и присваивание
    Settings(Settings const&) = delete;
    void operator=(Settings const&) = delete;

    Q_INVOKABLE void setValue(const QString &path, const QVariant &value);
    Q_INVOKABLE QVariant getValue(const QString &path, const QVariant &defaultValue = QVariant()) const;

    bool saveSettings(const QString &filePath);
    bool loadSettings(const QString &filePath);

    void saveSettings();
    void loadSettings();

    void setSettingsFilePath(const QString &path);

    void refresh();

    int fontSize() const;
    void setFontSize(int size);

    int fontDelta() const;
    void setFontDelta(int delta);

    QFont font(FONT_SIZE) const;

signals:
    void valueNotFound(const QString &warning);

private:
    QJsonObject settingsObject;
    QString settingsFilePath;
    QString settingFileDir;

    // Приватный конструктор
    explicit Settings(const QString& rootPath, QObject *parent = nullptr);

    int m_fontSize;
    int m_fontDelta;

    QFont sFont;
    QFont mFont;
    QFont bFont;
};

#endif // SETTINGSMANAGER_H
