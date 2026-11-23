#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

/**
    @file  core_engine.h
    @brief Настоящее ядро: цепочка DroneLink → MessageParser → LoggerState,
           DroneCommands, ReconArea — соединённые по схеме «Классов ядра», разд. 4.

    Устроено как заглушка CoreStub: те же методы и сигналы. DroneCore
    выбирает, с кем работать: «STUB» — заглушка, имя COM-порта — CoreEngine.

    @note  Живёт в потоке ядра (DroneCore переносит его через moveToThread).
           Все классы цепочки — его дочерние объекты и переезжают вместе с ним.
    @note  Пока без спектров, тревог, базы, SD, пересчёта на 1 м, сетки и экспорта —
           они подключаются на следующих шагах.
*/

#include <QObject>
#include <QVector>

#include "core_types.h"

class DroneLink;
class MessageParser;
class DroneCommands;
class LoggerState;

class CoreEngine : public QObject
{
    Q_OBJECT

public:
    /**
        @param link Для проверки (engine_test) — подставной DroneLink, владелец — вызывающий.
                    nullptr (обычно) — CoreEngine создаёт свой.
    */
    explicit CoreEngine(DroneLink *link = nullptr, QObject *parent = nullptr);

    // ---------------------------------------------------------------- вызовы из DroneCore
    // Выполняются в потоке ядра.

    void openPort(const QString &name);                 ///< открыть COM-порт; состояние логгера сбрасывается
    void closePort();
    void sendCommand(const CommandRequest &request);    ///< → DroneCommands
    void checkArea(const QVector<GeoPoint> &vertices, bool isRoute);   ///< → ReconArea, ответ areaChecked
    void applySettings(const CoreSettings &settings);   ///< раздать настройки классам

    QVector<GeoPoint> acceptedArea() const { return m_acceptedArea; }   ///< область, принятая логгером (ID01 на ID04)

signals:
    // Те же сигналы, что у DroneCore (часть) — описание в drone_core.h
    void linkStatusChanged(const LinkStatus &status);
    void loggerStatusChanged(const LoggerStatus &status);
    void allowedCommandsChanged(const QList<quint8> &ids);
    void routePoint(const RoutePoint &point);
    void measurement(const Measurement &m);
    void event(const CoreEvent &e);
    void coreError(const QString &source, const QString &text);
    void commandFinished(const CommandOutcome &outcome);
    void areaChecked(const AreaCheck &check);

private:
    void onCommandFinished(const CommandOutcome &outcome);
    void onPortError(const QString &text);
    void sendEvent(EventCode code, const QVariantMap &params = {});

    DroneLink     *m_link = nullptr;
    MessageParser *m_parser = nullptr;
    DroneCommands *m_commands = nullptr;
    LoggerState   *m_state = nullptr;

    double            m_maxAreaKm = 50.0;   ///< из настроек
    QVector<GeoPoint> m_acceptedArea;       ///< принятая логгером область
};

#endif // CORE_ENGINE_H
