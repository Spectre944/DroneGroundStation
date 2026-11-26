#include "csvexport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

#include "backend/settings/settings.h"
#include "backend/tables/tablemodel.h"
#include "ui/modules/indicatorPanel/indicatoritem.h"   // StatusColor
#include "ui/pages/database/databasepage.h"            // формат строки полёта

namespace {
const QString KEY_FOLDER = QStringLiteral("csv/lastFolder");
constexpr qint64 AllFlights = 0;        ///< значение пункта «Усі польоти» в списке
}

CsvExportWindow::CsvExportWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::Window);
    setWindowTitle(tr("Експорт у CSV"));
    resize(620, 560);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);

    // ---- полёт ----
    auto *flightRow = new QHBoxLayout();
    flightRow->addWidget(new QLabel(tr("Політ:"), this));
    m_flight = new QComboBox(this);
    m_flight->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    flightRow->addWidget(m_flight, 1);
    auto *refresh = new QPushButton(tr("Оновити"), this);
    flightRow->addWidget(refresh);
    root->addLayout(flightRow);

    // ---- данные выбранного полёта — как в «База даних» ----
    m_model = new StickyTableModel(this);
    m_view = new StickyTableView(this);
    m_view->setModel(m_model);
    m_view->setFont(QFont("Consolas"));
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->horizontalHeader()->setStretchLastSection(true);
    root->addWidget(m_view, 1);

    // ---- что выгружать ----
    auto *partsBox = new QGroupBox(tr("Що експортувати"), this);
    auto *pl = new QGridLayout(partsBox);
    m_meas    = new QCheckBox(tr("Вимірювання"), partsBox);
    m_track   = new QCheckBox(tr("Маршрут"), partsBox);
    m_spectra = new QCheckBox(tr("Спектри"), partsBox);
    m_events  = new QCheckBox(tr("Події"), partsBox);
    for (QCheckBox *c : {m_meas, m_track, m_spectra, m_events}) {
        c->setChecked(true);
        connect(c, &QCheckBox::toggled, this, &CsvExportWindow::updateState);
    }
    pl->addWidget(m_meas, 0, 0);
    pl->addWidget(m_track, 0, 1);
    pl->addWidget(m_spectra, 1, 0);
    pl->addWidget(m_events, 1, 1);
    auto *hint = new QLabel(tr("Формат (роздільник, мова заголовків) — «Налаштування → Експорт CSV»"), partsBox);
    hint->setStyleSheet(QStringLiteral("color:%1").arg(StatusColor::muted().name()));
    hint->setWordWrap(true);
    pl->addWidget(hint, 2, 0, 1, 2);
    root->addWidget(partsBox);

    // ---- куда ----
    auto *folderRow = new QHBoxLayout();
    folderRow->addWidget(new QLabel(tr("Папка:"), this));
    m_folder = new QLineEdit(this);
    m_folder->setText(Settings::instance().getValue(KEY_FOLDER).toString());
    if (m_folder->text().isEmpty())
        m_folder->setText(QDir::toNativeSeparators(defaultFolder()));
    folderRow->addWidget(m_folder, 1);
    m_browse = new QPushButton(tr("Огляд…"), this);
    folderRow->addWidget(m_browse);
    root->addLayout(folderRow);

    // ---- ход и кнопки ----
    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->hide();
    root->addWidget(m_progress);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto *buttons = new QHBoxLayout();
    m_openFolder = new QPushButton(tr("Відкрити папку"), this);
    m_openFolder->hide();
    buttons->addWidget(m_openFolder);
    buttons->addStretch(1);
    m_export = new QPushButton(tr("Експортувати"), this);
    m_export->setDefault(true);
    m_export->setMinimumWidth(130);
    buttons->addWidget(m_export);
    root->addLayout(buttons);

    connect(refresh, &QPushButton::clicked, this, [this] {
        m_waitList = true;
        emit flightsRequested();
    });
    connect(m_flight, &QComboBox::currentIndexChanged, this, [this] {
        updatePreview();
        updateState();
    });
    connect(m_folder, &QLineEdit::textChanged, this, &CsvExportWindow::updateState);
    connect(m_browse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Папка для CSV"), m_folder->text());
        if (!dir.isEmpty())
            m_folder->setText(QDir::toNativeSeparators(dir));
    });
    connect(m_openFolder, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_target));
    });
    connect(m_export, &QPushButton::clicked, this, &CsvExportWindow::startExport);

    fillCombo();
    updateState();
}

QString CsvExportWindow::defaultFolder()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("export"));
}

/**
    @brief Показать окно и обновить список полётов.
    @param sessionId — выбрать этот полёт, когда список придёт (-1 — оставить выбор)
*/
void CsvExportWindow::open(qint64 sessionId)
{
    if (sessionId > 0)
        m_select = sessionId;
    if (!isBusy()) {
        m_waitList = true;
        emit flightsRequested();
    }
    show();
    raise();
    activateWindow();
}

void CsvExportWindow::setCurrentSession(qint64 id)
{
    m_currentSession = id;
    updatePreview();
}

/////////////////////////////////// Список полётов ///////////////////////////////////

void CsvExportWindow::onSessionsListed(const QVector<SessionInfo> &list)
{
    if (!m_waitList)
        return;                         // список запросил кто-то другой
    m_waitList = false;
    m_flights = list;
    fillCombo();
}

void CsvExportWindow::fillCombo()
{
    qint64 want = m_select > 0 ? m_select : m_flight->currentData().toLongLong();
    m_select = -1;

    QSignalBlocker block(m_flight);
    m_flight->clear();
    if (m_flights.isEmpty()) {
        m_flight->addItem(tr("Польотів немає"), -1);
    } else {
        m_flight->addItem(tr("Усі польоти (%1)").arg(m_flights.size()), AllFlights);
        for (const SessionInfo &s : std::as_const(m_flights))
            m_flight->addItem(QStringLiteral("№%1 · %2 · %3").arg(s.id).arg(s.name,
                                  s.startTime.toLocalTime().toString("dd.MM.yyyy HH:mm")), s.id);
        const int i = m_flight->findData(want);
        m_flight->setCurrentIndex(i >= 0 ? i : (m_flights.isEmpty() ? 0 : 1));   // по умолчанию — последний полёт
    }
    block.unblock();
    updatePreview();
    updateState();
}

void CsvExportWindow::updatePreview()
{
    m_model->clearData();
    const qint64 id = m_flight->currentData().toLongLong();
    QVector<QVector<QVariant>> rows;
    for (const SessionInfo &s : std::as_const(m_flights))
        if (id == AllFlights || s.id == id)
            rows.append(DatabasePage::flightRow(s, s.id == m_currentSession));
    if (rows.isEmpty())
        rows.append({QString(), m_waitList ? tr("Завантаження…") : tr("Польотів немає")});
    m_model->addSection(id == AllFlights ? tr("Усі польоти") : tr("Політ"), DatabasePage::flightHeaders(), rows,
                        palette().color(QPalette::Base), false);
    m_view->resizeColumnsToContents();
    m_view->resizeRowsToContents();
}

/////////////////////////////////// Экспорт ///////////////////////////////////

quint32 CsvExportWindow::parts() const
{
    quint32 p = 0;
    if (m_meas->isChecked())    p |= ExMeasurements;
    if (m_track->isChecked())   p |= ExTrack;
    if (m_spectra->isChecked()) p |= ExSpectra;
    if (m_events->isChecked())  p |= ExEvents;
    return p;
}

void CsvExportWindow::updateState()
{
    const bool busy = isBusy();
    const bool hasFlight = m_flight->currentData().toLongLong() >= 0 && !m_flights.isEmpty();
    m_export->setEnabled(!busy && hasFlight && parts() != 0 && !m_folder->text().trimmed().isEmpty());
    for (QWidget *w : std::initializer_list<QWidget *>{m_flight, m_meas, m_track, m_spectra, m_events, m_folder, m_browse})
        w->setEnabled(!busy);
    if (!busy && parts() == 0)
        m_status->setText(QStringLiteral("<span style='color:%1'>%2</span>")
                              .arg(StatusColor::warning().name(), tr("Оберіть, що експортувати")));
}

void CsvExportWindow::startExport()
{
    const qint64 id = m_flight->currentData().toLongLong();
    m_queue.clear();
    for (const SessionInfo &s : std::as_const(m_flights))
        if (id == AllFlights || s.id == id)
            m_queue << s;
    if (m_queue.isEmpty())
        return;

    m_target = QDir::fromNativeSeparators(m_folder->text().trimmed());
    Settings::instance().setValue(KEY_FOLDER, m_folder->text().trimmed());
    Settings::instance().saveSettings();

    m_total = m_queue.size();
    m_done = 0;
    m_errors.clear();
    m_progress->setRange(0, m_total);
    m_progress->setValue(0);
    m_progress->show();
    m_openFolder->hide();
    sendNext();
}

void CsvExportWindow::sendNext()
{
    if (m_queue.isEmpty()) {
        finish();
        return;
    }
    m_sending = m_queue.takeFirst();
    m_waiting = true;
    m_status->setText(tr("Експорт %1 з %2: «%3»…").arg(m_done + 1).arg(m_total).arg(m_sending.name.toHtmlEscaped()));
    updateState();
    emit exportRequested(m_sending.id, m_target, parts());
}

/**
    @note Ответ без номера полёта — относится к тому, что отправлен последним (m_sending).
          Заглушка отвечает сразу «готово» и файлов не пишет.
*/
void CsvExportWindow::onExportFinished(bool ok, const QString &path)
{
    if (!m_waiting)
        return;                         // ответ на быстрый экспорт из «База даних»
    m_waiting = false;
    ++m_done;
    m_progress->setValue(m_done);
    if (!ok)
        m_errors << QStringLiteral("«%1»: %2").arg(m_sending.name.toHtmlEscaped(), path.toHtmlEscaped());
    sendNext();
}

void CsvExportWindow::finish()
{
    m_progress->hide();
    const int okCount = m_total - m_errors.size();
    QString text;
    if (m_errors.isEmpty()) {
        text = QStringLiteral("<span style='color:%1'>%2</span>")
                   .arg(StatusColor::ok().name(),
                        m_total == 1 ? tr("Збережено в %1").arg(QDir::toNativeSeparators(m_target).toHtmlEscaped())
                                     : tr("Експортовано польотів: %1 · папка %2")
                                           .arg(m_total).arg(QDir::toNativeSeparators(m_target).toHtmlEscaped()));
    } else {
        text = QStringLiteral("<span style='color:%1'>%2</span><br>%3")
                   .arg(StatusColor::alarm().name(),
                        tr("Експортовано %1 з %2, помилки:").arg(okCount).arg(m_total),
                        m_errors.join(QStringLiteral("<br>")));
    }
    m_status->setText(text);
    m_openFolder->setVisible(okCount > 0);
    updateState();
}
