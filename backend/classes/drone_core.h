#ifndef DRONE_CORE_H
#define DRONE_CORE_H

/**
    @file  drone_core.h
    @brief Фасад ядра НПО «ДРОН-1» — единственная точка связи интерфейса с ядром.

    Интерфейс вызывает методы DroneCore и получает его сигналы.
    К COM-порту и базе интерфейс напрямую не обращается.

    @note  Потоки. Объект DroneCore живёт в потоке интерфейса и владеет потоком ядра.
           Классы ядра работают в потоке ядра. Методы DroneCore передают вызов в поток
           ядра и сразу возвращаются (интерфейс не ждёт). Ответы приходят сигналами.
*/

#include <QObject>
#include <QThread>
#include <QList>
#include <QStringList>

#include "core_types.h"

class DroneCore : public QObject
{
    Q_OBJECT

public:
    /**
        @brief Создаёт ядро: регистрирует типы, запускает поток ядра.
        @note  Создаётся один раз, в потоке интерфейса (MainWindow).
    */
    explicit DroneCore(QObject *parent = nullptr);

    /**
        @brief Останавливает поток ядра и дожидается его завершения.
    */
    ~DroneCore() override;

    DroneCore(const DroneCore &) = delete;
    DroneCore &operator=(const DroneCore &) = delete;

    // ---------------------------------------------------------------- Связь (Ф1)

    /**
        @brief  Список COM-портов компьютера.
        @return Имена портов, например «COM5».
        @note   Синхронный вызов: QSerialPortInfo безопасен в любом потоке.
    */
    QStringList availablePorts() const;

    /**
        @brief Открыть COM-порт радиомодема (115200, 8N1).
        @param name Имя порта.
        @note  Результат — сигнал linkStatusChanged; ошибка — coreError.
    */
    void openPort(const QString &name);

    /**
        @brief Закрыть COM-порт.
    */
    void closePort();


    // ---------------------------------------------------------------- Команды (Ф2) и область (Ф3)

    /**
        @brief  Отправить команду ID01–ID14.
        @param  request Команда; поле seq заполняет ядро.
        @return Номер запроса seq — по нему интерфейс узнаёт ответ в commandFinished.
    */
    quint32 sendCommand(CommandRequest request);

    /**
        @brief Проверить область разведки.
        @param vertices Вершины.
        @param isRoute  true — нарисована линия (маршрут): логгер её не поддерживает.
        @note  Результат — сигнал areaChecked.
    */
    void checkArea(const QVector<GeoPoint> &vertices, bool isRoute = false);

    // ---------------------------------------------------------------- Сессии (Ф12) и архив

    /**
        @brief Начать полёт («Новый полёт»).
        @param name Имя полёта (место, задание).
    */
    void startSession(const QString &name);

    /**
        @brief Закончить полёт («Завершить полёт»).
    */
    void finishSession();

    /**
        @brief Запросить список прошлых полётов. Ответ — sessionsListed.
    */
    void listSessions();

    /**
        @brief Открыть прошлый полёт для просмотра. Ответ — sessionData.
        @param sessionId Номер полёта.
    */
    void openSession(qint64 sessionId);

    /**
        @brief Поиск по архиву (страница «База даних»). Ответ — archiveResult.
        @param filter Условия поиска.
    */
    void queryArchive(const SessionFilter &filter);

    /**
        @brief Запросить данные для графиков по времени (Ф11). Ответ — timeSeriesReady.
        @param sessionId Номер полёта.
    */
    void requestTimeSeries(qint64 sessionId);

    // ---------------------------------------------------------------- SD (Ф13) и экспорт (Ф14)

    /**
        @brief Импорт и сверка файлов SD. Ход — sdProgress, итог — sdImported.
        @param files Файлы ГГММДД/ЧЧММСС.CSV.
    */
    void importSd(const QStringList &files);

    /**
        @brief Экспорт полёта в CSV. Итог — exportFinished.
        @param sessionId Номер полёта.
        @param folder    Папка для файлов.
        @param parts     Что выгружать — флаги ExportPart.
    */
    void exportCsv(qint64 sessionId, const QString &folder, quint32 parts = ExAll);

    // ---------------------------------------------------------------- Настройки и тревоги

    /**
        @brief Передать настройки ядру — при запуске и после каждого изменения.
    */
    void applySettings(const CoreSettings &settings);

    /**
        @brief Оператор увидел тревогу. Ответ — alarmAcknowledged.
        @param alarmId Номер тревоги (Alarm::id).
    */
    void acknowledgeAlarm(qint64 alarmId);

signals:
    // ---------------------------------------------------------------- по ходу полёта

    void linkStatusChanged(const LinkStatus &status);       ///< раз в секунду и при изменении связи
    void loggerStatusChanged(const LoggerStatus &status);   ///< изменилось состояние логгера
    void allowedCommandsChanged(const QList<quint8> &ids);  ///< команды, разрешённые сейчас (ID 1–14)
    void routePoint(const RoutePoint &point);               ///< новая точка маршрута
    void measurement(const Measurement &m);                 ///< новое измерение (уже с ПАЕД на 1 м)
    void spectrumUpdated(const Spectrum &spectrum);         ///< новая порция; portionList — только последняя
    void spectrumFinished(const Spectrum &spectrum);        ///< набор окончен; portionList — все порции
    void gridUpdated(const GridData &grid);                 ///< обновилась тепловая карта
    void alarm(const Alarm &a);                             ///< новая тревога
    void event(const CoreEvent &e);                         ///< событие для журнала
    void coreError(const QString &source, const QString &text);  ///< ошибка порта («port») или базы («db»)

    // ---------------------------------------------------------------- ответы на запросы

    void commandFinished(const CommandOutcome &outcome);    ///< ответ на команду, «нет ответа» или отказ
    void areaChecked(const AreaCheck &check);               ///< результат checkArea
    void sessionStarted(const SessionInfo &info);           ///< полёт начат
    void sessionFinished(const SessionInfo &info);          ///< полёт закончен
    void sessionsListed(const QVector<SessionInfo> &list);  ///< список прошлых полётов
    void sessionData(const SessionData &data);              ///< данные прошлого полёта
    void archiveResult(const ArchiveResult &result);        ///< результат поиска по архиву
    void timeSeriesReady(qint64 sessionId, const QVector<Measurement> &series);  ///< данные для Ф11
    void sdProgress(int percent);                           ///< ход импорта SD, 0–100
    void sdImported(const SdReport &report);                ///< импорт SD окончен
    void exportFinished(bool ok, const QString &path);      ///< экспорт окончен
    void alarmAcknowledged(qint64 alarmId);                 ///< оператор отметил тревогу

private:
    QThread      m_thread;          ///< поток ядра: в нём работают все классы ядра
    CoreSettings m_settings;        ///< последние настройки от интерфейса
    quint32      m_nextSeq = 1;     ///< следующий номер запроса команды
};

#endif // DRONE_CORE_H
