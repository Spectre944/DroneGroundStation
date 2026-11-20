#ifndef INDICATORITEM_H
#define INDICATORITEM_H

#include <QWidget>
#include <QFrame>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QVector>

/**
    @brief Одно состояние индикатора: иконка, цвет, название и пояснение.
    @note  Из этих же записей строится всплывающая подсказка — список всех состояний
           с подсветкой текущего, поэтому описание состояния задаётся в одном месте.
*/
struct IndicatorState {
    int     id = 0;
    QString icon;           ///< SVG из ресурсов; currentColor в файле заменяется на color
    QColor  color;          ///< цвет иконки; невалидный — цвет текста палитры (как IconHelper)
    QString title;          ///< короткое название состояния
    QString description;    ///< что это значит для оператора
};

/**
    @brief Цвета состояний — одни и те же во всех модулях.
    @note  Подобраны так, чтобы читаться и на светлой, и на тёмной теме.
*/
namespace StatusColor {
    inline QColor ok()       { return QColor(0x2E, 0x9E, 0x6A); }
    inline QColor info()     { return QColor(0x3D, 0x8B, 0xFD); }
    inline QColor warning()  { return QColor(0xD9, 0x96, 0x1A); }
    inline QColor alarm()    { return QColor(0xE5, 0x48, 0x4D); }
    inline QColor spectrum() { return QColor(0x8E, 0x6C, 0xEF); }
    inline QColor muted()    { return QColor(0x8A, 0x96, 0xA3); }
}

/**
    @brief Компактный индикатор: иконка 20 px в кнопке 32×32.
    @note  При наведении показывает IndicatorTip вместо стандартного QToolTip:
           в нём иконки перекрашены под тему, подсвечено текущее состояние
           и есть живая строка подробностей (setState(id, detail)).
*/
class IndicatorItem : public QWidget
{
    Q_OBJECT

public:
    explicit IndicatorItem(const QString &name, QWidget *parent = nullptr);
    ~IndicatorItem() override;

    void setName(const QString &name);
    void setStates(const QVector<IndicatorState> &states);
    void setState(int id, const QString &detail = QString());

    QString name() const                        { return m_name; }
    QString detail() const                      { return m_detail; }
    int state() const                           { return m_current; }
    const QVector<IndicatorState> &states() const { return m_states; }
    const IndicatorState *currentState() const;

    QSize sizeHint() const override             { return QSize(32, 32); }

    /// SVG → QPixmap заданного цвета с учётом devicePixelRatio.
    static QPixmap renderSvg(const QString &path, const QColor &color, int px, qreal dpr);

signals:
    void clicked();

protected:
    bool event(QEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    QPixmap iconFor(const IndicatorState &s, int px);

    QString                 m_name;
    QVector<IndicatorState> m_states;
    int                     m_current = -1;
    QString                 m_detail;
    bool                    m_hover = false;
    QHash<QString, QPixmap> m_cache;    ///< ключ: путь|цвет|размер
};

/**
    @brief Всплывающая подсказка индикатора — одна на всё приложение.
*/
class IndicatorTip : public QFrame
{
    Q_OBJECT

public:
    static void showFor(IndicatorItem *item);
    static void hideFor(IndicatorItem *item);
    static void refresh(IndicatorItem *item);   ///< перестроить, если сейчас показана для item

private:
    explicit IndicatorTip();
    void rebuild(IndicatorItem *item);
    void placeNear(IndicatorItem *item);

    IndicatorItem *m_owner = nullptr;
};

#endif // INDICATORITEM_H
