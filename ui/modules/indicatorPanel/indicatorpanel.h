#ifndef INDICATORPANEL_H
#define INDICATORPANEL_H

#include <QFrame>

#include "backend/classes/core_types.h"
#include "indicatoritem.h"

/**
    @brief Компактная панель состояния (верхний правый угол поверх карты).
    @note  Каждый значок — IndicatorItem; при наведении подсказка со всеми
           состояниями и подсветкой текущего. Слоты подключаются к сигналам DroneCore:
           linkStatusChanged, loggerStatusChanged, sessionStarted / sessionFinished,
           sessionData (info) — см. connectCore() в mainwindow.
*/
class IndicatorPanel : public QFrame
{
    Q_OBJECT

public:
    enum class Item { Link, Mode, Recon, Spectrum, Radio, Battery, NoSd, Mismatch, Session, Count };
    Q_ENUM(Item)

    explicit IndicatorPanel(QWidget *parent = nullptr);

    /// Пороги цвета батареи, В. TODO: согласовать с BATTERY_CRITICAL_V логгера.
    void setBatteryThresholds(float lowV, float criticalV);

public slots:
    void onLinkStatus(const LinkStatus &status);
    void onLoggerStatus(const LoggerStatus &status);
    void onSessionStarted(const SessionInfo &info);
    void onSessionFinished(const SessionInfo &info);
    void onSessionOpened(const SessionInfo &info);  ///< из sessionData(data.info) — режим просмотра

signals:
    void itemClicked(IndicatorPanel::Item item);

protected:
    void changeEvent(QEvent *e) override;

private:
    enum SessionState { SessNone, SessRecording, SessViewing };

    void buildStates();         ///< тексты состояний — заново при смене языка
    void applyAll();            ///< выставить состояния значков по последним данным
    void applyLink();
    void applyLogger();
    void applySession();

    QString deviceName(quint8 code) const;
    QString errorText(const LoggerStatus &s) const;

    IndicatorItem *item(Item i) const { return m_items[int(i)]; }

    IndicatorItem *m_items[int(Item::Count)] = {};

    LinkStatus   m_link;
    LoggerStatus m_logger;
    SessionInfo  m_session;
    SessionState m_sessionState = SessNone;

    float m_battLowV = 14.8f;       ///< ниже — жёлтый (предварительно, 4S Li-ion)
    float m_battCritV = 14.0f;      ///< ниже — красный (предварительно)
};

#endif // INDICATORPANEL_H
