#ifndef ALARMCENTER_H
#define ALARMCENTER_H

#include <QFrame>
#include <QVector>

#include "backend/classes/core_types.h"

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QScrollArea;

/**
    @brief Центр тревог: список тревог полёта, новые сверху, кнопка «Бачу».
    @note  Показывается в шторке у правого края карты (AlarmDrawer).
           Подключение:
             DroneCore::alarm             → onAlarm
             DroneCore::alarmAcknowledged → onAlarmAcknowledged
             DroneCore::sessionData       → setAlarms(data.alarms)
             acknowledgeRequested         → DroneCore::acknowledgeAlarm
           «Бачу» отмечается сразу (не ждёт ответа ядра) — подтверждение ядра просто совпадёт.
*/
class AlarmCenter : public QFrame
{
    Q_OBJECT

public:
    explicit AlarmCenter(QWidget *parent = nullptr);

    int unacknowledgedCount() const;
    const QVector<Alarm> &alarms() const { return m_alarms; }

    static QString alarmText(const Alarm &a);

public slots:
    void onAlarm(const Alarm &a);
    void onAlarmAcknowledged(qint64 alarmId);
    void setAlarms(const QVector<Alarm> &alarms);
    void acknowledgeAll();
    void clear();

signals:
    void alarmAdded(const Alarm &a);        ///< новая тревога (для мигания шторки и тоста)
    void acknowledgeRequested(qint64 alarmId);
    void showOnMapRequested(double lat, double lon);
    void unacknowledgedCountChanged(int count);
    void closeRequested();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    void rebuild();
    QWidget *makeRow(const Alarm &a);
    void acknowledge(qint64 id);

    QVector<Alarm> m_alarms;        ///< в порядке поступления
    qint64         m_localId = -1;  ///< id для тревог без номера в базе (id == 0)
    int            m_lastCount = -1;

    QLabel      *m_badge   = nullptr;
    QPushButton *m_ackAll  = nullptr;
    QToolButton *m_close   = nullptr;
    QScrollArea *m_scroll  = nullptr;
    QWidget     *m_list    = nullptr;
    QVBoxLayout *m_listLay = nullptr;
    QLabel      *m_empty   = nullptr;
};

#endif // ALARMCENTER_H
