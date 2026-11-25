#ifndef PAGEBAR_H
#define PAGEBAR_H

/**
    @file  pagebar.h
    @brief Вертикальная панель страниц у левого края окна (как в VS Code).
    @note  Кнопка = значок + подпись под ним; нажатая кнопка подсвечена акцентом.
           Сами действия (QAction, checkable) создаёт и обрабатывает MainWindow:
           панель только показывает их. Значки — SVG из ресурсов, перекрашиваются
           под тему (светлая / тёмная) при смене палитры.
           Рабочие страницы — сверху (addPage), «Налаштування» — прижата вниз (addBottomPage).
*/

#include <QHash>
#include <QToolBar>

class PageBar : public QToolBar
{
    Q_OBJECT
public:
    explicit PageBar(QWidget *parent = nullptr);

    void addPage(QAction *action, const QString &svgPath);         ///< сверху, по порядку
    void addBottomPage(QAction *action, const QString &svgPath);   ///< снизу, под растяжкой

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupButton(QAction *action);
    void applyTheme();

    QAction *m_spacer = nullptr;            ///< растяжка между верхними и нижними кнопками
    QHash<QAction *, QString> m_icons;      ///< действие → путь к SVG
    bool m_styling = false;                 ///< защита от PaletteChange изнутри setStyleSheet
};

#endif // PAGEBAR_H
