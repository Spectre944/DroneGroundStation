#ifndef EVENTLOG_H
#define EVENTLOG_H

#include <QWidget>
#include <QDateTime>
#include <QTextCharFormat>
#include <QVector>

#include "backend/classes/core_types.h"

class QPlainTextEdit;
class QCheckBox;

/**
    @brief Журнал событий: QPlainTextEdit, одна строка — одно событие.
    @note  Формат строки: «14:33:12 [ALERT] Тривога: ПАЕД 3,42 мкЗв/год вище порогу».
           Тег окрашен по уровню. Ядро присылает коды (EventCode, CommandOutcome, Alarm),
           текст на языке интерфейса подставляется здесь через tr().
           Отладочные события (ID70–ID84) хранятся всегда, показываются по галочке.
*/
class EventLog : public QWidget
{
    Q_OBJECT

public:
    enum class Level { Info, Cmd, Warn, Alert, Error, Debug };
    Q_ENUM(Level)

    explicit EventLog(QWidget *parent = nullptr);

    void setShowDebug(bool on);
    bool showDebug() const          { return m_showDebug; }

    void setUtc(bool on);           ///< время в UTC (иначе — местное)
    void setMaxEntries(int n);      ///< по умолчанию 5000; старые строки отбрасываются

public slots:
    void append(EventLog::Level level, const QString &text, const QDateTime &time = QDateTime());

    void onEvent(const CoreEvent &e);
    void onCommandFinished(const CommandOutcome &o);
    void onAlarm(const Alarm &a);
    void onCoreError(const QString &source, const QString &text);
    void loadSession(const SessionData &data);  ///< прошлый полёт: события, команды и тревоги по времени

    void clear();

protected:
    void changeEvent(QEvent *e) override;

private:
    struct Entry {
        QDateTime time;
        Level     level = Level::Info;
        QString   text;
    };

    void updateFormats();
    void render(const Entry &e);
    void rerender();

    QString tagFor(Level l) const;
    QString commandText(const CommandOutcome &o, Level *level) const;
    QString commandName(quint8 id) const;
    QString commandValue(const CommandRequest &r) const;
    QString rejectText(RejectReason r) const;
    QString eventText(const CoreEvent &e, Level *level) const;
    QString alarmText(const Alarm &a) const;

    QPlainTextEdit  *m_view = nullptr;
    QCheckBox       *m_debugBox = nullptr;

    QVector<Entry>   m_entries;
    int              m_max = 5000;
    bool             m_showDebug = false;
    bool             m_utc = false;

    QTextCharFormat  m_fmtTime;
    QTextCharFormat  m_fmtText;
    QTextCharFormat  m_fmtTag[6];   ///< индекс — Level
};

#endif // EVENTLOG_H
