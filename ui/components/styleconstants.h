#ifndef STYLECONSTANTS_H
#define STYLECONSTANTS_H

#include <QSize>

class StyleConstants {
public:

    static const int menuMinimimWidht = 150;

    static const int leftMenuMinimumBtnHeight = 50;
    static const int leftMenuMinimumBtnWidth = 50;

    static const int leftMenuIconSize = 20;

    static const int textEditControlHeight = 40;
    static const int textEditMinimumHeight = 150;
    static const int textEditIconSize = 20;

    static const int comboBoxControlHeight = 40;
    static const int comboBoxControlWidth = 150;

    static const int lineEditControlHeight = 40;

    static const int comboBoxIconSize = 20;

    static const int indicatorIconSize = 20;

    static const int cbrnElementMinH = 40;
    static const int cbrnElementMinW = 100;

    static const int settingsElementMinH = 40;
    static const int settingsElementMinW = 200;

    const QString errorColor        = "#FF0000";
    const QString warningColor      = "#FFFF00";
    const QString acceptColor       = "#00FF00";
    const QString attentionColor    = "#FF7700";

    const QString errorTextStyle        = QString(" color: %1; ").arg(errorColor);
    const QString warningTextStyle      = QString(" color: %1; ").arg(warningColor);
    const QString acceptTextStyle       = QString(" color: %1; ").arg(acceptColor);
    const QString attentionTextStyle    = QString(" color: %1; ").arg(attentionColor);

    const QString errorBoxStyle         = QString(" border: 2px solid %1; ").arg(errorColor);
    const QString warningBoxStyle       = QString(" border: 2px solid %1; ").arg(warningColor);
    const QString acceptBoxStyle        = QString(" border: 2px solid %1; ").arg(acceptColor);
    const QString attentionBoxStyle     = QString(" border: 2px solid %1; ").arg(attentionColor);
};


#endif // STYLECONSTANTS_H
