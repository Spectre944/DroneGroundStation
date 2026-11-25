#ifndef RECONPAGE_H
#define RECONPAGE_H

/**
    @file  reconpage.h
    @brief Страница «Розвідка» — область и параметры разведки, команда ID04 (Р2-3).
    @note  Источник правды об области — карта (область разведки в GeometryStore):
             - нарисовали на карте / выбрали кнопкой поля → MapWidget::reconAssigned → onAreaAssigned;
             - ввели координаты в поле → areaEdited → MapWidget::setReconArea → снова reconAssigned.
           Проверку делает ядро (DroneCore::checkArea → areaChecked → onAreaChecked).
           Поля параметров заполняются типовыми значениями со страницы «Налаштування»
           (ReconParams); правка здесь относится к одному полёту и в настройки не пишется.
           Кнопка «Відправити» активна, только если область прошла проверку, ID04 разрешена,
           нет ожидания ответа на другую команду и не открыт прошлый полёт.
*/

#include <QWidget>

#include "backend/classes/core_types.h"
#include "map/mapcore.h"
#include "reconparams.h"

namespace Ui {
class ReconPage;
}

class ReconPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReconPage(QWidget *parent = nullptr);
    ~ReconPage();

    /// Параметры из полей страницы (без вершин)
    ReconParams params() const;

public slots:
    /**
        @brief Новые типовые значения (при запуске и после «Зберегти» в настройках).
        @note  Поля, которые оператор не менял, принимают новые значения;
               изменённые вручную остаются как есть.
    */
    void setDefaults(const ReconParams &defaults);

    // --- от карты ---
    void onAreaAssigned(const QVector<smappoint> &points, bool isRoute);  ///< MapWidget::reconAssigned
    void onAreaCleared();                                                  ///< MapWidget::reconCleared

    // --- от DroneCore ---
    void onAreaChecked(const AreaCheck &check);
    void onAllowedCommands(const QList<quint8> &ids);
    void onLinkStatus(const LinkStatus &status);
    void onLoggerStatus(const LoggerStatus &status);
    void onCommandSent(quint32 seq);                    ///< номер запроса от DroneCore::sendCommand
    void onCommandFinished(const CommandOutcome &outcome);
    void setReadOnly(bool readOnly);                    ///< открыт прошлый полёт — отправка недоступна

    // --- полёт (Р2-2) ---
    void onSessionStarted(const SessionInfo &info);
    void onSessionFinished(const SessionInfo &info);

signals:
    void areaEdited(const QVector<smappoint> &points);  ///< координаты из поля → MapWidget::setReconArea
    void sendRequested(const CommandRequest &request);  ///< ID04 → DroneCore::sendCommand (seq → onCommandSent)

    /**
        @brief Начать полёт (DroneCore::startSession) — перед первой ID04.
        @note  Шлётся прямо перед sendRequested: ядро выполняет вызовы по порядку,
               поэтому область и данные разведки попадают уже в новый полёт.
    */
    void startFlightRequested(const QString &name);
    void finishFlightRequested();                       ///< DroneCore::finishSession

private:
    void connections();
    void applyParams(const ReconParams &p);
    void onCoordinatesEdited();
    void simplifyArea();
    void send();

    void updateAreaStatus();
    void updateDefaultsHint();
    void updateSendState();

    /// Проверка относится к текущей области (вершины совпадают)
    bool checkMatchesArea() const;

    Ui::ReconPage *ui;

    ReconParams        m_defaults;
    QVector<smappoint> m_area;          ///< текущая область разведки (с карты)
    bool               m_isRoute = false;
    AreaCheck          m_check;
    bool               m_checked = false;

    QList<quint8> m_allowed;
    LinkStatus    m_link;
    LoggerStatus  m_logger;
    bool          m_readOnly = false;
    quint32       m_pendingSeq = 0;     ///< ждём итог своей ID04
    bool          m_sending = false;    ///< sendRequested отправлен, номер ещё не пришёл
    QVector<CommandOutcome> m_early;    ///< итоги, пришедшие раньше номера (заглушка в том же потоке)
    QString       m_resultHtml;         ///< итог последней ID04 — под кнопкой
    void showOutcome(const CommandOutcome &outcome);
    bool          m_settingText = false;///< поле заполняется с карты — не отправлять обратно

    // Полёт: текущая сессия базы и начата ли она оператором (а не ядром автоматически)
    SessionInfo   m_session;
    bool          m_hasSession = false;
    bool          m_ownSession = false;
    QString       m_requestedName;      ///< имя, с которым просили startSession
    void updateFlightUi();
    QString flightName() const;         ///< «Назва · Завдання» или дата и время
};

#endif // RECONPAGE_H
