#ifndef TABLEMODEL_H
#define TABLEMODEL_H

#include <QMainWindow>
#include <QTableView>
#include <QScrollBar>
#include <QEvent>
#include <QAbstractTableModel>
#include <QHeaderView>
#include <QVector>
#include <QMap>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

struct TableSection {
    QString name;
    QStringList headers;
    int startRow;
    int rowCount;
    bool isSticky;
    QColor backgroundColor;  // Цвет фона для секции
    bool addSeparator;      // Флаг для добавления разделителя
};

class StickyTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit StickyTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Обновленный метод добавления секции с параметрами оформления
    void addSection(const QString& name,
                    const QStringList& headers,
                    const QVector<QVector<QVariant>>& data,
                    const QColor& bgColor = Qt::white,
                    bool addSeparator = true);

    void clearData();
    void updateVisibleHeaders(int firstVisibleRow);
    const QVector<TableSection>& getSections() const { return sections; }

private:
    QVector<TableSection> sections;
    QVector<QVector<QVariant>> tableData;
    QStringList currentVisibleHeaders;
    int lastVisibleSection;

    TableSection* findSectionForRow(int row) const;
    void updateHeaderView();
    bool isSeparatorRow(int row) const;  // Проверка, является ли строка разделителем
};

//---------------------------------------------------------------------------//
//                                                                           //
//---------------------------------------------------------------------------//

class SimpleTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:

    struct CellData {
        QString text;
        bool isError = false;

        CellData() = default;
        CellData(const QString &t, bool error = false)
            : text(t), isError(error) {}

        // Для совместимости с QVariant
        operator QVariant() const {
            return QVariant::fromValue(*this);
        }
    };


    explicit SimpleTableModel(QObject *parent = nullptr);

    // Обязательные методы QAbstractTableModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Метод для редактирования данных
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Методы для управления данными
    void addRow(const QVector<QVariant>& rowData);
    void clearData();
    void setHeaderDataFromList(const QStringList& list);
    void updateData(const QVector<QVector<QVariant>>& data);

private:
    QVector<QVector<QVariant>> tableData;
    QStringList horizontalHeaders;
};

//---------------------------------------------------------------------------//
//                                                                           //
//---------------------------------------------------------------------------//

class StickyTableView : public QTableView
{
    Q_OBJECT
public:
    explicit StickyTableView(QWidget *parent = nullptr) : QTableView(parent) {
        verticalScrollBar()->installEventFilter(this);
        this->horizontalHeader()->setStretchLastSection(true);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj == verticalScrollBar()) {
            if (event->type() == QEvent::Wheel ||
                event->type() == QEvent::MouseMove ||
                event->type() == QEvent::MouseButtonPress ||
                event->type() == QEvent::MouseButtonRelease) {
                updateHeaders();
            }
        }
        return QTableView::eventFilter(obj, event);
    }

    void scrollContentsBy(int dx, int dy) override {
        QTableView::scrollContentsBy(dx, dy);
        if (dy != 0) {
            updateHeaders();
        }
    }

private:
    void updateHeaders() {
        auto model = qobject_cast<StickyTableModel*>(this->model());
        if (model) {
            int firstVisibleRow = rowAt(viewport()->rect().top());
            if (firstVisibleRow >= 0) {
                model->updateVisibleHeaders(firstVisibleRow);
            }
        }
    }
};


//---------------------------------------------------------------------------//
//                                                                           //
//---------------------------------------------------------------------------//


class TableNavigator : public QWidget
{
    Q_OBJECT

public:


    explicit TableNavigator(StickyTableView* tableView, QWidget* parent = nullptr);
    ~TableNavigator();



public slots:
    void updateActiveSection();
    void updateSectionList();

private slots:
    void scrollToSection(int row)
    {
        tableView->scrollTo(tableView->model()->index(row, 0),
                            QAbstractItemView::PositionAtTop);
    }

private:
    StickyTableView* tableView;
    QVBoxLayout* layout;
    QVector<QPushButton*> buttons;

    void setupUI();
    void fillData();
    void connections();
    void clearButtonList();
};

//---------------------------------------------------------------------------//
//                                                                           //
//---------------------------------------------------------------------------//

#endif // TABLEMODEL_H
