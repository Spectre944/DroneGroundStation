#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

/**
    @file  alarm_manager.h
    @brief Тревоги (Ф8): ПАЕД выше порога, авария дрона, ошибка SD,
           потеря связи, «режим не совпадает».

    Главное правило — одна тревога на одну ситуацию: не на каждой точке пятна,
    а один раз при входе в зону. Повтор — только после выхода и нового входа.

    @note  AlarmManager только решает, нужна ли тревога. Номер тревоге даёт база:
           CoreEngine записывает её (SessionStore::saveAlarm) и отправляет наружу.
    @note  Живёт в потоке ядра, в CoreEngine.
*/

#include <QObject>

#include "core_types.h"

class AlarmManager : public QObject
{
    Q_OBJECT

public:
    explicit AlarmManager(QObject *parent = nullptr);

    /**
        @brief Измерение: пороги ПАЕД, вход в зону.
        @note  Сравнивается shownPaed(): ПАЕД на 1 м, если посчитана, иначе исходная.
               Недостоверное измерение (флаги Ф9) тревогу не вызывает.
    */
    void onMeasurement(const Measurement &m);

    void onRoutePoint(const RoutePoint &p);         ///< запомнить последнее положение — для «На карті»
    void onLoggerError(quint8 msgId);               ///< авария: ID24, ID25, ID26, ID28 (от LoggerState::enteredError)
    void onEvent(const CoreEvent &e);               ///< ошибка SD (ID16)
    void onLinkStatus(const LinkStatus &status);    ///< потеря и восстановление связи
    void onModeMismatch(const CommandRequest &request);    ///< ID03 на разрешённую команду

    void setThresholds(float warn, float alarm);    ///< пороги ПАЕД из настроек, мкЗв/ч
    void reset();                                   ///< новый сеанс связи: всё «ниже порога», связь «не терялась»

signals:
    void alarm(const Alarm &a);                     ///< новая тревога (без номера) → CoreEngine → база → наружу
    void event(const CoreEvent &e);                 ///< «связь потеряна / восстановлена» → журнал

private:
    void raise(AlarmType type, AlarmLevel level, const QDateTime &time,
               float value = 0.0f, quint8 msgId = 0);
    void sendEvent(EventCode code);

    float    m_warn = 0.3f;
    float    m_alarm = 1.0f;

    bool     m_aboveWarn = false;       ///< сейчас выше порога предупреждения
    bool     m_aboveAlarm = false;      ///< сейчас выше порога тревоги
    bool     m_errorRaised = false;     ///< тревога об аварии уже выдана
    bool     m_sdRaised = false;        ///< тревога об ошибке SD уже выдана
    bool     m_linkLost = false;        ///< связь сейчас потеряна (тревога выдана)

    bool     m_hasPos = false;          ///< положение известно
    GeoPoint m_pos;                     ///< последнее положение дрона
};

#endif // ALARM_MANAGER_H
