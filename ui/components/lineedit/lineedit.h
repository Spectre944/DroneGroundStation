#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <QWidget>
#include <QMenu>
#include <QLabel>
#include <QAction>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QContextMenuEvent>

#include <ui/components/styleconstants.h>
#include <backend/coords/coordhandler.h>

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
class LineEditReport : public QWidget
{
    Q_OBJECT

public:
    LineEditReport(QWidget *parent = nullptr);
    void loadComboBoxData(const QString &filePath);

private slots:
    void onComboBoxItemSelected(const QString &item);

private:
    QLineEdit *lineEdit;
    QComboBox *comboBox;
    QHash<QString, QString> abbreviations;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
class LineEditButton : public QWidget {
    Q_OBJECT

public:
    LineEditButton(QWidget *parent = nullptr);

private slots:
    void onActionButtonClicked();
    void onAutoFillSelected();
    void onManualInputSelected();

private:
    QLineEdit *lineEdit;
    QPushButton *actionButton;
    QMenu *menu;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
class LineEditMenu : public QLineEdit {
    Q_OBJECT

public:
    LineEditMenu(QWidget *parent = nullptr);

private slots:

private:

    QMenu *m_Menu;
    QPushButton *m_autofillBtn;
    QHBoxLayout *hLayout;

    // QWidget interface
protected:
    //void contextMenuEvent(QContextMenuEvent *event);
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
class LineEditCoordinates : public QWidget {
    Q_OBJECT

public:
    LineEditCoordinates(QWidget *parent = nullptr);
    QString text();

    enum class WidgetType {
        ComboBox,
        LineEdit
    };

    QWidget* getWidget(WidgetType type) const;

private slots:
    void showContextMenu(const QPoint &pos);

private:
    void setupUI();
    void fillData();
    void connections();
    void convertCoordinates();
    void setValidationError(const QString &errorText);

    QComboBox   *m_comboBox;
    QLineEdit   *m_lineEdit;
    QMenu       *m_Menu;
    QLabel      *m_warningLabel;
    QHBoxLayout *hLayout;
    QGridLayout *m_lineEditLayout;

};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//
class LineEditAutofill : public QWidget
{
    Q_OBJECT

public:

    enum class WidgetType {
        LineEdit,
        PushButton
    };

    LineEditAutofill(QWidget *parent = nullptr);
    QWidget* getWidget(WidgetType type) const;

signals:

private:
    void setupUI();
    void fillData();

    QLineEdit   *m_lineEdit;
    QPushButton *m_pushButtonAutofill;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class LineEditDisplayCoordinates : public QLineEdit {
    Q_OBJECT
public:
    using QLineEdit::QLineEdit; // Наследуем конструкторы

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        emit doubleClicked();
        QLineEdit::mousePressEvent(event);
    }
};

#endif // LINEEDIT_H
