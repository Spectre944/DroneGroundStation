#include "tablemodel.h"

StickyTableModel::StickyTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void StickyTableModel::addSection(const QString& name,
                                  const QStringList& headers,
                                  const QVector<QVector<QVariant>>& data,
                                  const QColor& bgColor,
                                  bool addSeparator)
{
    beginResetModel();

    // Добавляем разделитель перед новой секцией, если это не первая секция
    if (addSeparator && !tableData.isEmpty()) {
        tableData.append(QVector<QVariant>()); // Пустая строка-разделитель
    }

    TableSection section;
    section.name = name;
    section.headers = headers;
    section.startRow = tableData.size();
    section.rowCount = data.size();
    section.backgroundColor = bgColor;
    section.addSeparator = addSeparator;

    sections.append(section);
    tableData.append(data);

    if (sections.size() == 1) {
        currentVisibleHeaders = headers;
        lastVisibleSection = 0;
    }

    endResetModel();
}

bool StickyTableModel::isSeparatorRow(int row) const
{
    for (const auto& section : sections) {
        if (section.addSeparator && row == section.startRow - 1) {
            return true;
        }
    }
    return false;
}

int StickyTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return tableData.size();
}

int StickyTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    int maxColumns = 0;
    for (const auto& section : sections) {
        maxColumns = qMax(maxColumns, section.headers.size());
    }
    return maxColumns;
}

QVariant StickyTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // Обработка разделительной строки
    if (isSeparatorRow(index.row())) {
        switch (role) {
        case Qt::BackgroundRole:
            return QColor(Qt::lightGray);  // Цвет разделителя
        case Qt::DisplayRole:
            return QString();  // Пустая строка
        default:
            return QVariant();
        }
    }

    // Определяем секцию для текущей строки
    TableSection* section = findSectionForRow(index.row());

    if (role == Qt::BackgroundRole && section) {
        return section->backgroundColor;
    }

    if ((role == Qt::DisplayRole || role == Qt::EditRole) && section) {
        const auto& row = tableData[index.row()];
        if (index.column() < row.size()) {
            return row[index.column()];
        }
    }

    return QVariant();
}

Qt::ItemFlags StickyTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || isSeparatorRow(index.row()))
        return Qt::NoItemFlags;

    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void StickyTableModel::updateVisibleHeaders(int firstVisibleRow)
{
    TableSection* section = findSectionForRow(firstVisibleRow);
    if (section) {
        int sectionIndex = section - &sections[0];
        if (sectionIndex != lastVisibleSection) {
            lastVisibleSection = sectionIndex;
            currentVisibleHeaders = section->headers;
            updateHeaderView();
        }
    }
}

void StickyTableModel::updateHeaderView()
{
    emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
}

TableSection* StickyTableModel::findSectionForRow(int row) const
{
    for (int i = sections.size() - 1; i >= 0; --i) {
        if (row >= sections[i].startRow) {
            return const_cast<TableSection*>(&sections[i]);
        }
    }
    return nullptr;
}

QVariant StickyTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        if (section < currentVisibleHeaders.size()) {
            return currentVisibleHeaders[section];
        }
    } else if (orientation == Qt::Vertical) {
        return section + 1;
    }

    return QVariant();
}

bool StickyTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    if (index.row() >= 0 && index.row() < tableData.size() &&
        index.column() >= 0 && index.column() < tableData[index.row()].size()) {
        tableData[index.row()][index.column()] = value;
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

void StickyTableModel::clearData()
{
    beginResetModel();
    sections.clear();
    tableData.clear();
    endResetModel();
}

//---------------------------------------------------------------------------//
//                                                                           //
//---------------------------------------------------------------------------//

SimpleTableModel::SimpleTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    // Инициализация заголовков (пример)
    horizontalHeaders << "СВНГ-Т" << "СРХБР-Т" << "ChemPro" << "AirMar" << "Параметри" << "Збереження";
}

int SimpleTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return tableData.size();
}

int SimpleTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return horizontalHeaders.size();
}

QVariant SimpleTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= 0 && index.row() < tableData.size() &&
        index.column() >= 0 && index.column() < tableData[index.row()].size()) {

        const QVariant &cellVariant = tableData[index.row()][index.column()];

        // Проверяем, содержит ли ячейка структурированные данные
        if (cellVariant.canConvert<CellData>()) {
            CellData cellData = cellVariant.value<CellData>();

            if (role == Qt::DisplayRole || role == Qt::EditRole) {
                return cellData.text;
            }

            if (role == Qt::ForegroundRole) {
                if (cellData.isError) {
                    return QBrush(Qt::red);
                }
                return QVariant();
            }

        } else {
            // Обычные данные без флагов
            if (role == Qt::DisplayRole || role == Qt::EditRole) {
                return cellVariant;
            }
        }
    }

    return QVariant();
}

QVariant SimpleTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal && section < horizontalHeaders.size()) {
        return horizontalHeaders[section];
    }

    if (orientation == Qt::Vertical) {
        return section + 1;
    }

    return QVariant();
}

bool SimpleTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    if (index.row() >= 0 && index.row() < tableData.size() &&
        index.column() >= 0 && index.column() < tableData[index.row()].size()) {
        tableData[index.row()][index.column()] = value;
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

Qt::ItemFlags SimpleTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void SimpleTableModel::addRow(const QVector<QVariant>& rowData)
{
    beginInsertRows(QModelIndex(), tableData.size(), tableData.size());
    tableData.append(rowData);
    endInsertRows();
}

void SimpleTableModel::clearData()
{
    beginResetModel();
    tableData.clear();
    endResetModel();
}

void SimpleTableModel::setHeaderDataFromList(const QStringList &list)
{
    horizontalHeaders.clear();
    horizontalHeaders = list;
}

void SimpleTableModel::updateData(const QVector<QVector<QVariant>> &data)
{
    beginResetModel();
    tableData.append(data);
    endResetModel();
}

//---------------------------------------------------------------------------//
//                                                                           //
//---------------------------------------------------------------------------//


TableNavigator::TableNavigator(StickyTableView *tableView, QWidget *parent)
    : QWidget(parent), tableView(tableView)
{
    setupUI();
    fillData();
    connections();

}

TableNavigator::~TableNavigator()
{
    clearButtonList();
}

void TableNavigator::setupUI()
{
    setWindowFlags(Qt::Window);
    setGeometry(100, 100, 200, 200);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setWindowTitle("Навігація по таблиці");

    layout = new QVBoxLayout(this);
    layout->setSpacing(2);
    layout->setContentsMargins(5, 5, 5, 5);
    //layout->addStretch();

}

void TableNavigator::fillData()
{
    updateSectionList();
}

void TableNavigator::connections()
{
    connect(tableView->verticalScrollBar(), &QScrollBar::valueChanged, this, &TableNavigator::updateActiveSection);
}

void TableNavigator::clearButtonList()
{
    foreach(QPushButton *btn, buttons) {
        if(btn){
            delete btn;
        }
    }
    buttons.clear();
}

void TableNavigator::updateActiveSection()
{
    int firstVisibleRow = tableView->rowAt(tableView->viewport()->rect().top());

    if (firstVisibleRow < 0) return;

    auto model = qobject_cast<StickyTableModel*>(tableView->model());
    if (!model) return;

    const auto& sections = model->getSections();
    for (int i = 0; i < buttons.size(); ++i) {
        bool isActive = false;
        if (i < sections.size() - 1) {
            isActive = (firstVisibleRow >= sections[i].startRow &&
                        firstVisibleRow < sections[i + 1].startRow);
        } else {
            isActive = (firstVisibleRow >= sections[i].startRow);
        }

        buttons[i]->setProperty("active", isActive);
        buttons[i]->style()->unpolish(buttons[i]);
        buttons[i]->style()->polish(buttons[i]);
    }
}

void TableNavigator::updateSectionList()
{
    clearButtonList();

    auto model = qobject_cast<StickyTableModel*>(tableView->model());
    if (model) {
        const auto& sections = model->getSections();
        for (const auto& section : sections) {
            auto button = new QPushButton(section.name, this);
            button->setProperty("active", false);
            button->setProperty("buttonRole", "navigationButton");
            button->setCursor(Qt::PointingHandCursor);
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

            connect(button, &QPushButton::clicked, this, [this, section]() {
                scrollToSection(section.startRow);
            });

            buttons.append(button);
            layout->addWidget(button);
        }
    }
}

