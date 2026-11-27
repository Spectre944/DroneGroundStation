#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

/**
    @file  toggleswitch.h
    @brief Переключатель «вимк / увімк»: прямоугольник с закруглёнными углами и ползунком.
    @note  Положение показывает ФАКТИЧЕСКОЕ состояние (setOn — например, от LoggerStatus),
           а не последний клик: клик только просит сменить состояние (сигнал toggleRequested),
           сам переключатель не переключается. Так он не врёт, если команда не прошла.
           Пока ждём ответ — setPending(true): ползунок полупрозрачный, клики игнорируются.
           Цвета — от палитры (тема Windows), «увімкнено» — цвет accent (по умолчанию Highlight).
*/

#include <QAbstractButton>
#include <QColor>

class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    void setOn(bool on);
    bool isOn() const { return m_on; }

    void setPending(bool pending);
    bool isPending() const { return m_pending; }

    /// Подписи на дорожке: слева при «увімк», справа при «вимк»
    void setLabels(const QString &on, const QString &off);
    void setAccent(const QColor &c) { m_accent = c; update(); }

    QSize sizeHint() const override;

signals:
    void toggleRequested(bool wantOn);

protected:
    void paintEvent(QPaintEvent *) override;
    void nextCheckState() override {}      ///< не переключаться самому

private:
    bool    m_on = false;
    bool    m_pending = false;
    QString m_onText = tr("УВІМК");
    QString m_offText = tr("ВИМК");
    QColor  m_accent;
};

#endif // TOGGLESWITCH_H
