#ifndef DIALOG_H
#define DIALOG_H

#include <QWidget>
#include <QDialog>
#include <QDateEdit>
#include <QDateTime>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>

#include <ui/components/textedit/textedit.h>

class Dialog : public QWidget
{
    Q_OBJECT
public:
    explicit Dialog(QWidget *parent = nullptr);

signals:
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class DateDateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DateDateDialog(QWidget *parent = nullptr);

    // Функция для получения выбранных дат
    QPair<QDate, QDate> selectedDates() const {
        return qMakePair(dateEditFrom->date(), dateEditTo->date());
    }

signals:
    void dateDateReady(qint64 timeFrom, qint64 timeTo);

private:
    QDateTimeEdit *dateEditFrom;
    QDateTimeEdit *dateEditTo;
    QPushButton *buttonOk;
    QPushButton *buttonCancel;
};

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

class AreaSearchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AreaSearchDialog(QWidget *parent = nullptr);

signals:
    void areaReady(QString);

private:
    TextEditCoordinates *editCoordinates;
    QPushButton *buttonOk;
    QPushButton *buttonCancel;
};

#endif // DIALOG_H
