#include "dialog.h"

Dialog::Dialog(QWidget *parent)
    : QWidget{parent}
{}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

DateDateDialog::DateDateDialog(QWidget *parent)
{
    // Q_UNUSED(parent);
    QGridLayout *glayout = new QGridLayout(this);

    dateEditFrom = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    dateEditTo = new QDateTimeEdit(QDateTime::currentDateTime(), this);

    dateEditFrom->setCalendarPopup(true);
    dateEditTo->setCalendarPopup(true);

    buttonOk = new QPushButton("OK", this);
    buttonCancel = new QPushButton("Відміна", this);

    connect(buttonOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(buttonCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(buttonOk, &QPushButton::clicked, this, [this](){
        // Получаем QDateTime из QDateTimeEdit
        QDateTime fromDateTime = dateEditFrom->dateTime();
        QDateTime toDateTime = dateEditTo->dateTime();

        // Конвертируем в секунды с 1970 года
        qint64 fromSeconds = fromDateTime.toSecsSinceEpoch();
        qint64 toSeconds = toDateTime.toSecsSinceEpoch();

        emit dateDateReady(fromSeconds, toSeconds);
    });

    glayout->addWidget(new QLabel("Від:", this), 0, 0);
    glayout->addWidget(dateEditFrom, 0, 1);
    glayout->addWidget(new QLabel("До:", this), 1, 0);
    glayout->addWidget(dateEditTo, 1, 1);
    glayout->addWidget(buttonOk, 2, 0);
    glayout->addWidget(buttonCancel, 2, 1);

    setLayout(glayout);
    setWindowTitle("Виберіть дату");
    //setModal(true);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setStyleSheet(parent->styleSheet());
}

//--------------------------------------------------------------------//
//                                                                    //
//--------------------------------------------------------------------//

AreaSearchDialog::AreaSearchDialog(QWidget *parent)
{
    // Q_UNUSED(parent);
    QGridLayout *glayout = new QGridLayout(this);

    editCoordinates = new TextEditCoordinates(this);
    editCoordinates->setWindowDesription("Вершини область пошуку");

    buttonOk = new QPushButton("OK", this);
    buttonCancel = new QPushButton("Відміна", this);

    connect(buttonOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(buttonCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(buttonOk, &QPushButton::clicked, this, [this](){
        emit areaReady(editCoordinates->getCoordinateString());
    });

    glayout->addWidget(editCoordinates, 0, 0, 1, 2);
    glayout->addWidget(buttonOk, 1, 0);
    glayout->addWidget(buttonCancel, 1, 1);

    setLayout(glayout);
    setWindowTitle("Виберіть площу");
    //setModal(true);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setStyleSheet(parent->styleSheet());
}
