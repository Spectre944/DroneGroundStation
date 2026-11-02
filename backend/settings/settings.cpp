#include <backend/settings/settings.h>

Settings::Settings(const QString &rootPath, QObject *parent)
{
    Q_UNUSED(parent);
    setSettingsFilePath(rootPath);
    refresh();

    int fontId = QFontDatabase::addApplicationFont(":/fonts/GigaSans/GigaSans-Medium.ttf");
    if (fontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);

        if (!fontFamilies.isEmpty()) {
            QString fontFamily = fontFamilies.at(0);

            QFont appFont(fontFamily);
            sFont = appFont;
            mFont = appFont;
            bFont = appFont;

            int fontSize_t = fontSize() | 10;
            int deltaSize_t = fontDelta() | 2;

            sFont.setPointSize(fontSize_t - deltaSize_t);
            mFont.setPointSize(fontSize_t);
            bFont.setPointSize(fontSize_t + deltaSize_t);

        }
    }
}

void Settings::setValue(const QString &path, const QVariant &value)
{
    QStringList pathComponents = path.split("/");

    QVector<QJsonObject> valuesUpToPath{settingsObject};

    // Ensure the path up to the last value exists,
    for (int i = 0; i < pathComponents.size() - 1; i++)
    {
        QString const &currentKey = pathComponents[i];

        if (!valuesUpToPath.last().contains(currentKey))
        {
            valuesUpToPath.last()[currentKey] = QJsonObject();
        }

        valuesUpToPath.push_back(valuesUpToPath.last()[currentKey].toObject());
    }

    // Set the last item's key = value
    valuesUpToPath.last()[pathComponents.last()] = QJsonValue::fromVariant(value);

    // Now merge all the items back into one
    QJsonObject result = valuesUpToPath.takeLast();

    while (valuesUpToPath.size() > 0)
    {
        QJsonObject parent = valuesUpToPath.takeLast();
        parent[pathComponents[valuesUpToPath.size()]] = result;
        result = parent;
    }

    settingsObject = result;
}

QVariant Settings::getValue(const QString &path, const QVariant &defaultValue) const
{
    QVariant result = defaultValue;

    QStringList pathComponents = path.split("/");
    QJsonObject parentObject = settingsObject;

    while (pathComponents.size() > 0)
    {
        QString const &currentKey = pathComponents.takeFirst();

        if (parentObject.contains(currentKey))
        {
            if (pathComponents.size() == 0)
            {
                result = parentObject.value(currentKey);
            }
            else
            {
                parentObject = parentObject.value(currentKey).toObject();
            }
        }
        else
        {
            QString warn = QStringLiteral("Settings could not access unknown key ") + currentKey + QStringLiteral(" in ") + QString::fromUtf8(QJsonDocument(parentObject).toJson());
            qWarning() << qPrintable(warn);
            return QVariant();
            //break;
        }
    }

    return result;
}

bool Settings::saveSettings(const QString &filePath)
{
    if (!filePath.isEmpty()) {
        settingsFilePath = filePath;
    }

    saveSettings();
    return true; // You may want to add error handling logic here
}

bool Settings::loadSettings(const QString &filePath)
{
    if (!filePath.isEmpty()) {
        settingsFilePath = filePath;
    }

    loadSettings();
    return true; // You may want to add error handling logic here
}

void Settings::saveSettings()
{
    QDir dir;
    if (!dir.exists(settingFileDir))
        dir.mkpath(settingFileDir);

    QFile file(settingsFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument jsonDoc(settingsObject);
        file.write(jsonDoc.toJson());
        file.close();
    }
}

void Settings::loadSettings()
{
    QFile file(settingsFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray jsonData = file.readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
        settingsObject = jsonDoc.object();
        file.close();
    }
}

void Settings::setSettingsFilePath(const QString &path)
{
    if (!path.isEmpty()) {
        settingFileDir = path + "/settings";
        settingsFilePath = settingFileDir + "/settings.json";
        loadSettings(); // Перезагружаем настройки при изменении пути
    }

}
//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
void Settings::refresh()
{
    m_fontSize = getValue("ui/font/size").toInt();
    m_fontDelta = getValue("ui/font/delta").toInt();
}
//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
int Settings::fontSize() const
{
    return m_fontSize;
}

void Settings::setFontSize(int size)
{
    m_fontSize = size;
    setValue("ui/font/size", size);
}
//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
int Settings::fontDelta() const
{
    return m_fontDelta;
}

void Settings::setFontDelta(int delta)
{
    m_fontDelta = delta;
    setValue("ui/font/delta", delta);
}

QFont Settings::font(FONT_SIZE size) const
{
    switch (size) {
        case FONT_SIZE::SMALL:  return sFont; break;
        case FONT_SIZE::MEDIUM: return mFont; break;
        case FONT_SIZE::BIG:    return bFont; break;
        default:                return QFont(); break;
    }
}
//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
