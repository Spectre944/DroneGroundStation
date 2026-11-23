#include "csv_exporter.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVector>
#include <algorithm>

namespace {
    const char Sep = ';';                       ///< разделитель полей

    /// Строка из полей через «;»
    QString row(const QStringList &fields) { return fields.join(QLatin1Char(Sep)); }
}

// ============================================================================
//  Выгрузка
// ============================================================================

bool CsvExporter::exportSession(const SessionData &data, const QString &folder, quint32 parts,
                                const CoreSettings &settings)
{
    m_files.clear();
    m_error.clear();
    m_dec = settings.csvDecimalSep;
    m_en  = (settings.language == QStringLiteral("en"));

    if (!QDir().mkpath(folder)) {
        m_error = QStringLiteral("Папка недоступна: %1").arg(folder);
        return false;
    }
    const QString base = QDir(folder).filePath(fileBase(data.info));

    if ((parts & ExMeasurements) && !writeFile(base + "_measurements.csv", measurementRows(data)))
        return false;
    if ((parts & ExTrack) && !writeFile(base + "_track.csv", trackRows(data)))
        return false;
    if (parts & ExSpectra) {
        for (int i = 0; i < data.spectra.size(); ++i) {
            const Spectrum &s = data.spectra[i];
            // Номер спектра логгера; спектр с SD рядом с тем же номером из эфира — с пометкой
            QString name = QString::number(s.info.number);
            if (s.info.source == DataSource::Sd)
                name += QStringLiteral("_sd");
            if (!writeFile(base + "_spectrum_" + name + ".csv", spectrumRows(s)))
                return false;
        }
    }
    if ((parts & ExEvents) && !writeFile(base + "_events.csv", eventRows(data)))
        return false;
    return true;
}

QString CsvExporter::fileBase(const SessionInfo &info)
{
    QString name = info.name;
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));   // недопустимое в Windows
    name = name.simplified().replace(QLatin1Char(' '), QLatin1Char('_'));
    return name.isEmpty() ? QStringLiteral("flight_%1").arg(info.id)
                          : QStringLiteral("flight_%1_%2").arg(info.id).arg(name);
}

bool CsvExporter::writeFile(const QString &path, const QStringList &rows)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = QStringLiteral("%1: %2").arg(path, f.errorString());
        return false;
    }
    f.write("\xEF\xBB\xBF");                    // BOM: иначе Excel покажет кириллицу кракозябрами
    for (const QString &r : rows) {
        f.write(r.toUtf8());
        f.write("\r\n");
    }
    m_files << path;
    return true;
}

// ============================================================================
//  Таблицы
// ============================================================================

QStringList CsvExporter::measurementRows(const SessionData &d) const
{
    QStringList rows;
    rows << row({ tr("Дата і час UTC", "Date and time UTC"), tr("Широта", "Latitude"), tr("Довгота", "Longitude"),
                  tr("Висота LIDAR, м", "LIDAR height, m"),
                  tr("ПАЕД, мкЗв/год", "Dose rate, uSv/h"), tr("ПАЕД на 1 м, мкЗв/год", "Dose rate at 1 m, uSv/h"),
                  tr("Температура БДБГ (код)", "BDBG temperature (code)"), tr("Стан БДБГ", "BDBG status"),
                  tr("Потужність сигналу LIDAR", "LIDAR signal strength"), tr("Напруга акумулятора, В", "Battery, V"),
                  tr("Достовірність", "Validity"), tr("Джерело", "Source") });

    for (const Measurement &m : d.measurements) {
        QStringList validity;
        if (m.flags & MfOutOfArea)   validity << tr("поза областю", "out of area");
        if (m.flags & MfLidarRange)  validity << tr("LIDAR", "LIDAR");
        if (m.flags & MfBdbgStat)    validity << tr("стан БДБГ", "BDBG status");
        if (m.flags & MfDeviceFault) validity << tr("відмова приладу", "device fault");

        rows << row({ time(m.time), num(m.lat, 6), num(m.lon, 6), num(m.lidarCm / 100.0, 2),
                      num(m.paed, 4), m.hasPaed1m ? num(m.paed1m, 4) : QString(),
                      QString::number(m.tempCode), QStringLiteral("0x%1").arg(m.bdbgStat, 2, 16, QLatin1Char('0')),
                      QString::number(m.lidarStrength), num(m.battV, 2),
                      validity.isEmpty() ? tr("так", "yes") : field(validity.join(QStringLiteral(", "))),
                      m.source == DataSource::Sd ? QStringLiteral("SD") : tr("ефір", "air") });
    }
    return rows;
}

QStringList CsvExporter::trackRows(const SessionData &d) const
{
    QStringList rows;
    rows << row({ tr("Дата і час UTC", "Date and time UTC"), tr("Широта", "Latitude"), tr("Довгота", "Longitude"),
                  tr("В області", "In area"), tr("Повідомлення", "Message"), tr("Джерело", "Source") });
    for (const RoutePoint &r : d.route) {
        rows << row({ time(r.time), num(r.lat, 6), num(r.lon, 6),
                      r.inArea ? tr("так", "yes") : tr("ні", "no"),
                      QStringLiteral("ID%1").arg(r.msgId),
                      r.source == DataSource::Sd ? QStringLiteral("SD") : tr("ефір", "air") });
    }
    return rows;
}

QStringList CsvExporter::spectrumRows(const Spectrum &s) const
{
    QString reason;
    switch (s.info.stopReason) {
    case SpectrumStopReason::None:          reason = tr("набір не закінчено", "not finished"); break;
    case SpectrumStopReason::Measurement:   reason = tr("логер повернувся до вимірювань", "logger resumed measurements"); break;
    case SpectrumStopReason::CommandStop:   reason = tr("команда ID12", "command ID12"); break;
    case SpectrumStopReason::CommandFinish: reason = tr("команда ID08", "command ID08"); break;
    case SpectrumStopReason::NewArea:       reason = tr("команда ID04", "command ID04"); break;
    case SpectrumStopReason::NewSpectrum:   reason = tr("новий спектр", "new spectrum"); break;
    case SpectrumStopReason::LoggerError:   reason = tr("аварія", "logger error"); break;
    case SpectrumStopReason::TimeElapsed:   reason = tr("минув заданий час", "time elapsed"); break;
    case SpectrumStopReason::ChannelFull:   reason = tr("канал заповнено (55705)", "channel full (55705)"); break;
    }

    QStringList rows;
    rows << row({ tr("Номер спектра", "Spectrum number"), QString::number(s.info.number) });
    rows << row({ tr("Початок UTC", "Start UTC"), time(s.info.startTime) });
    rows << row({ tr("Широта", "Latitude"), num(s.info.lat, 6) });
    rows << row({ tr("Довгота", "Longitude"), num(s.info.lon, 6) });
    rows << row({ tr("Висота LIDAR, м", "LIDAR height, m"), num(s.info.lidarCm / 100.0, 2) });
    rows << row({ tr("Порцій", "Portions"), QString::number(s.info.portions) });
    rows << row({ tr("Кінець набору", "Stop reason"), field(reason) });
    rows << row({ tr("Джерело", "Source"), s.info.source == DataSource::Sd ? QStringLiteral("SD") : tr("ефір", "air") });
    rows << QString();
    rows << row({ tr("Канал", "Channel"), tr("Відліки", "Counts") });
    for (int ch = 0; ch < s.channels.size(); ++ch)
        rows << row({ QString::number(ch), QString::number(s.channels[ch]) });
    return rows;
}

/**
    @brief События, команды и тревоги одним списком по времени.
    @note  Текст события — код и параметры: подробные тексты — у интерфейса.
*/
QStringList CsvExporter::eventRows(const SessionData &d) const
{
    struct Line { QDateTime t; QStringList f; };
    QVector<Line> lines;

    for (const CoreEvent &e : d.events) {
        const QString params = e.params.isEmpty() ? QString()
            : QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(e.params)).toJson(QJsonDocument::Compact));
        lines.append({ e.time, { time(e.time), e.debug ? tr("налагодження", "debug") : tr("подія", "event"),
                                 QStringLiteral("0x%1").arg(int(e.code), 2, 16, QLatin1Char('0')), field(params),
                                 e.source == DataSource::Sd ? QStringLiteral("SD") : tr("ефір", "air") } });
    }

    for (const CommandOutcome &c : d.commands) {
        QString result;
        switch (c.result) {
        case CommandResult::Done:      result = tr("виконано (ID01)", "done (ID01)"); break;
        case CommandResult::BadPacket: result = tr("помилка пакета (ID02)", "bad packet (ID02)"); break;
        case CommandResult::Forbidden: result = tr("заборонено (ID03)", "forbidden (ID03)"); break;
        case CommandResult::NoAnswer:  result = tr("немає відповіді", "no answer"); break;
        case CommandResult::Rejected:  result = tr("не відправлено", "not sent"); break;
        }
        lines.append({ c.sentTime, { time(c.sentTime), tr("команда", "command"),
                                     QStringLiteral("ID%1").arg(c.request.id), field(result), QString() } });
    }

    for (const Alarm &a : d.alarms) {
        QString type;
        switch (a.type) {
        case AlarmType::DoseRate:     type = tr("ПАЕД %1", "dose rate %1").arg(num(a.value, 3)); break;
        case AlarmType::LoggerError:  type = tr("аварія ID%1", "logger error ID%1").arg(a.msgId); break;
        case AlarmType::SdError:      type = tr("помилка SD", "SD error"); break;
        case AlarmType::LinkLost:     type = tr("зв'язок втрачено", "link lost"); break;
        case AlarmType::ModeMismatch: type = tr("режим не збігається", "mode mismatch"); break;
        }
        lines.append({ a.time, { time(a.time),
                                 a.level == AlarmLevel::Alarm ? tr("тривога", "alarm") : tr("попередження", "warning"),
                                 QString(), field(type),
                                 a.acknowledged ? tr("переглянуто", "acknowledged") : QString() } });
    }

    std::stable_sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) { return a.t < b.t; });

    QStringList rows;
    rows << row({ tr("Дата і час UTC", "Date and time UTC"), tr("Тип", "Type"), tr("Код", "Code"),
                  tr("Зміст", "Details"), tr("Примітка", "Note") });
    for (const Line &l : lines)
        rows << row(l.f);
    return rows;
}

// ============================================================================
//  Форматирование
// ============================================================================

QString CsvExporter::num(double v, int decimals) const
{
    QString s = QString::number(v, 'f', decimals);
    if (m_dec != QLatin1Char('.'))
        s.replace(QLatin1Char('.'), m_dec);
    return s;
}

QString CsvExporter::time(const QDateTime &t) const
{
    return t.isValid() ? t.toUTC().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")) : QString();
}

QString CsvExporter::field(const QString &s)
{
    if (!s.contains(QLatin1Char(Sep)) && !s.contains(QLatin1Char('"')) && !s.contains(QLatin1Char('\n')))
        return s;
    QString q = s;
    q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + q + QLatin1Char('"');
}
