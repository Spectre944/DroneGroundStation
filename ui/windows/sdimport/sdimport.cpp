#include "sdimport.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

// ======================================================================
//  SdDropArea
// ======================================================================

SdDropArea::SdDropArea(QWidget *parent)
    : QLabel(parent)
{
    setAcceptDrops(true);
    setAlignment(Qt::AlignCenter);
    setWordWrap(true);
    setMinimumHeight(120);
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::PointingHandCursor);
    setText(tr("Перетягніть сюди файли *.CSV або папки з SD-карти\nабо натисніть, щоб вибрати файли"));
    applyStyle(false);
}

void SdDropArea::applyStyle(bool highlighted)
{
    if (m_styling)
        return;
    m_styling = true;            // setStyleSheet сам шлёт PaletteChange — без повторного входа
    m_highlighted = highlighted;
    // Цвета — от палитры приложения (тема Windows), чтобы область читалась и в тёмной теме
    const QPalette pal = QApplication::palette();
    QColor bg = pal.color(QPalette::Base);
    QColor text = pal.color(QPalette::PlaceholderText);
    const QColor accent = pal.color(QPalette::Highlight);
    QColor border = pal.color(QPalette::Mid);
    if (highlighted) {
        border = accent;
        bg = QColor::fromRgbF(accent.redF(), accent.greenF(), accent.blueF(), 0.12);
        text = pal.color(QPalette::Text);
    }
    setStyleSheet(QStringLiteral("QLabel { border: 2px dashed %1; border-radius: 8px; background: %2; color: %3; padding: 16px; }")
                      .arg(border.name(), bg.name(QColor::HexArgb), text.name()));
    m_styling = false;
}

void SdDropArea::changeEvent(QEvent *e)
{
    QLabel::changeEvent(e);
    if (e->type() == QEvent::PaletteChange || e->type() == QEvent::ApplicationPaletteChange)
        applyStyle(m_highlighted);
}

void SdDropArea::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
        applyStyle(true);
        return;
    }
    e->ignore();
}

void SdDropArea::dragLeaveEvent(QDragLeaveEvent *e)
{
    applyStyle(false);
    e->accept();
}

void SdDropArea::dropEvent(QDropEvent *e)
{
    applyStyle(false);
    QStringList paths;
    for (const QUrl &u : e->mimeData()->urls())
        if (u.isLocalFile())
            paths << u.toLocalFile();
    const QStringList files = SdImport::collectCsv(paths);
    if (files.isEmpty()) {
        e->ignore();
        return;
    }
    e->acceptProposedAction();
    emit filesDropped(files);
}

void SdDropArea::mouseReleaseEvent(QMouseEvent *e)
{
    QLabel::mouseReleaseEvent(e);
    emit clicked();
}

void SdDropArea::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        emit clicked();
        return;
    }
    QLabel::keyPressEvent(e);
}

// ======================================================================
//  SdImport
// ======================================================================

SdImport::SdImport(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::Window);
    setWindowTitle(tr("Імпорт з SD-карти"));
    setMinimumWidth(480);
    m_lastDir = QCoreApplication::applicationDirPath();
    buildUi();
    updateButtons();
    resize(520, sizeHint().height());
}

QStringList SdImport::collectCsv(const QStringList &paths)
{
    QStringList out;
    for (const QString &p : paths) {
        const QFileInfo fi(p);
        if (fi.isDir()) {
            // Папка SD целиком или папка дня ГГММДД — все *.CSV внутри
            QDirIterator it(fi.absoluteFilePath(), {"*.csv", "*.CSV"}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
                out << it.next();
        } else if (fi.isFile() && fi.suffix().compare(QLatin1String("csv"), Qt::CaseInsensitive) == 0) {
            out << fi.absoluteFilePath();
        }
    }
    out.removeDuplicates();
    out.sort();          // ГГММДД/ЧЧММСС — по времени
    return out;
}

void SdImport::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);

    auto *hint = new QLabel(tr("Дані з SD-карти логера звіряються з тим, що прийшло по радіоканалу, "
                               "і доповнюють поточний політ (немає поточного — останній). "
                               "Файли: папки <b>ГГММДД</b> з файлами <b>ЧЧММСС.CSV</b>."), this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_drop = new SdDropArea(this);
    root->addWidget(m_drop);

    auto *pick = new QHBoxLayout();
    m_folder = new QPushButton(tr("Вибрати папку…"), this);
    m_clear = new QPushButton(tr("Очистити список"), this);
    pick->addWidget(m_folder);
    pick->addStretch();
    pick->addWidget(m_clear);
    root->addLayout(pick);

    m_listTitle = new QLabel(this);
    root->addWidget(m_listTitle);
    m_list = new QListWidget(this);
    m_list->setMinimumHeight(70);
    m_list->setMaximumHeight(160);
    root->addWidget(m_list);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->hide();
    root->addWidget(m_progress);

    m_import = new QPushButton(tr("Імпортувати"), this);
    m_import->setMinimumHeight(34);
    root->addWidget(m_import);

    // Отчёт
    auto *box = new QGroupBox(tr("Звіт"), this);
    m_reportBox = box;
    auto *rl = new QVBoxLayout(box);
    m_reportGrid = new QGridLayout();
    m_reportGrid->setHorizontalSpacing(16);
    rl->addLayout(m_reportGrid);
    m_details = new QPlainTextEdit(box);
    m_details->setReadOnly(true);
    m_details->setMaximumHeight(110);
    m_details->hide();
    rl->addWidget(m_details);
    m_openFlight = new QPushButton(tr("Переглянути політ"), box);
    m_openFlight->setToolTip(tr("Відкрити політ, у який додано дані з SD, на карті"));
    rl->addWidget(m_openFlight, 0, Qt::AlignLeft);
    box->hide();
    root->addWidget(box);

    connect(m_drop, &SdDropArea::filesDropped, this, &SdImport::addFiles);
    connect(m_drop, &SdDropArea::clicked, this, &SdImport::chooseFiles);
    connect(m_folder, &QPushButton::clicked, this, &SdImport::chooseFolder);
    connect(m_clear, &QPushButton::clicked, this, [this] {
        m_files.clear();
        m_list->clear();
        updateButtons();
    });
    connect(m_import, &QPushButton::clicked, this, &SdImport::startImport);
    connect(m_openFlight, &QPushButton::clicked, this, &SdImport::openFlightRequested);
}

void SdImport::addFiles(const QStringList &files)
{
    for (const QString &f : files) {
        if (m_files.contains(f))
            continue;
        m_files << f;
        // В списке — «ГГММДД/ЧЧММСС.CSV», полный путь — в подсказке
        const QFileInfo fi(f);
        auto *item = new QListWidgetItem(fi.dir().dirName() + QLatin1Char('/') + fi.fileName(), m_list);
        item->setToolTip(QDir::toNativeSeparators(f));
    }
    if (!files.isEmpty())
        m_lastDir = QFileInfo(files.first()).absolutePath();
    m_reportBox->hide();
    updateButtons();
}

void SdImport::chooseFiles()
{
    if (m_running)
        return;
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Файли з SD-карти"), m_lastDir,
                                                            tr("Файли логера (*.csv *.CSV);;Всі файли (*)"));
    addFiles(collectCsv(files));
}

void SdImport::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Папка SD-карти або дня (ГГММДД)"), m_lastDir);
    if (dir.isEmpty())
        return;
    const QStringList files = collectCsv({dir});
    if (files.isEmpty())
        m_listTitle->setText(tr("У папці немає файлів *.CSV"));
    addFiles(files);
}

void SdImport::startImport()
{
    if (m_files.isEmpty() || m_running)
        return;
    setRunning(true);
    m_progress->setValue(0);
    m_reportBox->hide();
    emit importRequested(m_files);
}

void SdImport::setRunning(bool running)
{
    m_running = running;
    m_progress->setVisible(running || m_progress->value() > 0);
    m_drop->setEnabled(!running);
    updateButtons();
}

void SdImport::updateButtons()
{
    m_listTitle->setText(m_files.isEmpty() ? tr("Файли не вибрано")
                                           : tr("Вибрано файлів: %1").arg(m_files.size()));
    m_import->setEnabled(!m_files.isEmpty() && !m_running);
    m_import->setText(m_running ? tr("Імпорт…") : tr("Імпортувати"));
    m_folder->setEnabled(!m_running);
    m_clear->setEnabled(!m_running && !m_files.isEmpty());
}

void SdImport::onSdProgress(int percent)
{
    m_progress->show();
    m_progress->setValue(qBound(0, percent, 100));
}

void SdImport::onSdImported(const SdReport &r)
{
    setRunning(false);
    m_progress->setValue(100);

    // Таблица отчёта: что значит каждая цифра — простыми словами
    while (QLayoutItem *it = m_reportGrid->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    struct Line { QString name; int value; QString hint; const char *color; };
    const QList<Line> lines = {
        {tr("Файлів оброблено"),      int(r.files.size()), QString(), nullptr},
        {tr("Збіглося з ефіром"),      r.matched,  tr("ці дані вже були"), nullptr},
        {tr("Тільки на SD — додано"),  r.onlySd,   tr("втрачені в радіоканалі, тепер у базі"), r.onlySd ? "#2E9E6A" : nullptr},
        {tr("Тільки в ефірі"),         r.onlyAir,  tr("нормально: частина повідомлень на SD не пишеться"), nullptr},
        {tr("Спектри не збіглися"),    r.spectraMismatch, tr("підсумок на SD ≠ сума порцій"), r.spectraMismatch ? "#B7791F" : nullptr},
        {tr("Помилкових рядків"),      r.badLines, tr("пропущено"), r.badLines ? "#E5484D" : nullptr},
    };
    int row = 0;
    for (const Line &l : lines) {
        auto *name = new QLabel(l.name, m_reportBox);
        auto *val = new QLabel(QString::number(l.value), m_reportBox);
        QFont f = val->font();
        f.setBold(true);
        val->setFont(f);
        if (l.color)
            val->setStyleSheet(QStringLiteral("color: %1").arg(QLatin1String(l.color)));
        auto *hint = new QLabel(l.hint, m_reportBox);
        hint->setProperty("textRole", "secondary");
        m_reportGrid->addWidget(name, row, 0);
        m_reportGrid->addWidget(val, row, 1, Qt::AlignRight);
        m_reportGrid->addWidget(hint, row, 2);
        ++row;
    }
    m_details->setPlainText(r.details.join(QLatin1Char('\n')));
    m_details->setVisible(!r.details.isEmpty());
    m_reportBox->show();
    // Окно растёт под отчёт, чтобы ничего не сжималось
    adjustSize();
    resize(qMax(width(), sizeHint().width()), qMax(height(), sizeHint().height()));
}
