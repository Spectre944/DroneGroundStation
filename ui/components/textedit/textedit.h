#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <ui/components/styleconstants.h>
#include <backend/coords/coordhandler.h>
#include <map/mapcore.h>
#include <backend/utilities/utilities.h>

class CoordinatesManager;

class TextEditCoordinates : public QWidget
{
    Q_OBJECT

public:

    enum class WidgetType {
        ComboBox,
        Button,
        TextEdit
    };

    enum class CoordinatesMode {
        Area,
        Route,
        Other
    };

    explicit TextEditCoordinates(QWidget *parent = nullptr);

    void setWindowDesription(const QString &title);
    void setPlaceholderText(const QString &text);
    void setCoordinatesMode(CoordinatesMode mode);
    QString getCoordinateString();
    bool isCoordinatesValid();
    int getCoordinatesCount();
    QWidget* getWidget(WidgetType type) const;
    CoordinatesMode getMode();

    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void requestCoordinates();
    void setCoordinatesFromMap(QVector<smappoint> coordinates);
    void convertCoordinates();
    void clearTextEditContent();
signals:
    void editingFinished();

private:
    void setupUI();
    void fillData();
    void connections();
    void setValidationError(const QString &errorText);
    bool m_coordinatesValid = false;
    int m_coordinatesCount = 0;

    QLabel      *m_label;
    QComboBox   *m_comboBox;
    QTextEdit   *m_textEdit;
    QLabel      *m_warningLabel;
    QPushButton *m_pushButton;
    QMenu       *m_Menu;
    QHBoxLayout *hLayout1;
    QHBoxLayout *hLayout2;
    QVBoxLayout *vLayout;
    QGridLayout *m_textEditLayout;
    CoordinatesMode _mode = CoordinatesMode::Area;


};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class TextEditAutofill : public QWidget
{
    Q_OBJECT

public:
    enum class WidgetType {
        TextEdit,
        PushButton
    };

    TextEditAutofill(QWidget *parent = nullptr);
    void setPlaceholderText(const QString &text);
    QWidget* getWidget(WidgetType type) const;

signals:

private:
    void setupUI();
    void fillData();

    QTextEdit   *m_textEdit;
    QPushButton *m_pushButtonAutofill;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // TEXTEDIT_H
