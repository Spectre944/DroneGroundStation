#include "reconpage.h"
#include "ui_reconpage.h"

#include <QDateTime>
#include <QMessageBox>
#include <utility>
#include <cmath>

#include "backend/coords/coordhandler.h"
#include "map/mapwidget.h"                    // mapgeo::simplifyPolygon, mapgeo::areaKm2
#include "ui/modules/coreTexts/coretexts.h"
#include "ui/modules/notifier/notifier.h"

namespace {

const char *kOk    = "#2E9E6A";
const char *kWarn  = "#B7791F";
const char *kError = "#E5484D";
const char *kMuted = "#8A96A3";

QString colored(const char *color, const QString &text)
{
    return QStringLiteral("<span style='color:%1'>%2</span>").arg(QLatin1String(color), text.toHtmlEscaped());
}

/// Совпадают ли вершины (с точностью ~0,1 м — поле координат хранит 6 знаков)
template <typename A, typename B, typename LatA, typename LonA, typename LatB, typename LonB>
bool samePoints(const QVector<A> &a, const QVector<B> &b, LatA latA, LonA lonA, LatB latB, LonB lonB)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i)
        if (std::fabs(latA(a[i]) - latB(b[i])) > 1e-6 || std::fabs(lonA(a[i]) - lonB(b[i])) > 1e-6)
            return false;
    return true;
}

} // namespace

ReconPage::ReconPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ReconPage)
{
    ui->setupUi(this);

    ui->textEditCoordinates->setCoordinatesMode(TextEditCoordinates::CoordinatesMode::Area);
    ui->textEditCoordinates->setWindowDesription(tr("Вершини області (3–20)"));
    ui->labelDefaults->setProperty("textRole", "secondary");
    ui->pushButtonSimplify->setText(tr("Спростити до %1 вершин").arg(CoreConst::AreaMaxVertices));
    ui->pushButtonSimplify->hide();

    ui->comboBoxReconMode->setToolTip(tr("Коли логер виконує вимірювання"));
    ui->spinBoxInterval->setToolTip(tr("Режим «за часом»: вимірювання через N секунд (логер вимагає > 0 у будь-якому режимі)"));
    ui->spinBoxDistance->setToolTip(tr("Режим «за відстанню»: вимірювання через N метрів (логер вимагає > 0 у будь-якому режимі)"));
    ui->spinBoxDup->setToolTip(tr("Скільки разів логер повторює кожен пакет, 2–5"));

    m_defaults = ReconParams::load();       // при запуске поля — всегда типовые значения
    applyParams(m_defaults);
    connections();
    updateAreaStatus();
    updateSendState();
    updateFlightUi();
}

ReconPage::~ReconPage()
{
    delete ui;
}

void ReconPage::connections()
{
    connect(ui->textEditCoordinates, &TextEditCoordinates::editingFinished, this, &ReconPage::onCoordinatesEdited);
    connect(ui->pushButtonSimplify, &QPushButton::clicked, this, &ReconPage::simplifyArea);
    connect(ui->pushButtonSend, &QPushButton::clicked, this, &ReconPage::send);
    connect(ui->pushButtonResetDefaults, &QPushButton::clicked, this, [this] { applyParams(m_defaults); });
    connect(ui->pushButtonFinishFlight, &QPushButton::clicked, this, [this] {
        const QString text = m_logger.mode == LoggerMode::Work
            ? tr("Розвідка ще триває. Якщо завершити політ зараз, наступні дані підуть в автоматичний запис.\n\nЗавершити політ «%1»?")
            : tr("Нові дані підуть у наступний політ.\n\nЗавершити політ «%1»?");
        if (QMessageBox::question(this, tr("Завершити політ"), text.arg(m_session.name)) == QMessageBox::Yes)
            emit finishFlightRequested();
    });

    // Любая правка поля — подсказка «змінено відносно типових»
    connect(ui->comboBoxReconMode, &QComboBox::currentIndexChanged, this, &ReconPage::updateDefaultsHint);
    for (QSpinBox *s : {ui->spinBoxInterval, ui->spinBoxDistance, ui->spinBoxSpectrum, ui->spinBoxDup})
        connect(s, &QSpinBox::valueChanged, this, &ReconPage::updateDefaultsHint);
}

// ------------------------------------------------------------------
//  Параметры
// ------------------------------------------------------------------

ReconParams ReconPage::params() const
{
    ReconParams p;
    p.reconMode = ReconMode(qBound(0, ui->comboBoxReconMode->currentIndex(), 2));
    p.intervalS = ui->spinBoxInterval->value();
    p.distanceM = ui->spinBoxDistance->value();
    p.spectrumS = ui->spinBoxSpectrum->value();
    p.dupFactor = ui->spinBoxDup->value();
    return p;
}

void ReconPage::applyParams(const ReconParams &p)
{
    ui->comboBoxReconMode->setCurrentIndex(int(p.reconMode) <= 2 ? int(p.reconMode) : 2);
    ui->spinBoxInterval->setValue(p.intervalS);
    ui->spinBoxDistance->setValue(p.distanceM);
    ui->spinBoxSpectrum->setValue(p.spectrumS);
    ui->spinBoxDup->setValue(p.dupFactor);
    updateDefaultsHint();
}

void ReconPage::setDefaults(const ReconParams &defaults)
{
    const bool untouched = params() == m_defaults;
    m_defaults = defaults;
    if (untouched)
        applyParams(defaults);
    updateDefaultsHint();
}

void ReconPage::updateDefaultsHint()
{
    const bool same = params() == m_defaults;
    ui->labelDefaults->setText(same ? tr("Типові значення з налаштувань")
                                    : tr("Змінено для цього польоту"));
    ui->labelDefaults->setToolTip(same ? QString()
                                       : tr("Зміни на цій сторінці в налаштуваннях не зберігаються"));
    ui->pushButtonResetDefaults->setEnabled(!same);
}

// ------------------------------------------------------------------
//  Область
// ------------------------------------------------------------------

void ReconPage::onCoordinatesEdited()
{
    if (m_settingText)
        return;                                 // это мы сами заполнили поле с карты

    const QString text = ui->textEditCoordinates->getCoordinateString().trimmed();
    if (text.isEmpty())
        return;
    if (!ui->textEditCoordinates->isCoordinatesValid()) {
        ui->labelAreaStatus->setText(colored(kError, tr("Невірний формат координат")));
        return;
    }
    const QPair<bool, QVector<smappoint>> parsed = CoordHandler::convertPackageCoordsToMapPoints(text);
    if (!parsed.first || parsed.second.isEmpty()) {
        ui->labelAreaStatus->setText(colored(kError, tr("Невірний формат координат")));
        return;
    }
    const auto lat = [](const smappoint &p) { return p.latt; };
    const auto lon = [](const smappoint &p) { return p.longt; };
    if (!m_isRoute && samePoints(parsed.second, m_area, lat, lon, lat, lon))
        return;                                 // ничего не изменилось

    emit areaEdited(parsed.second);             // карта назначит разведкой → onAreaAssigned
}

void ReconPage::onAreaAssigned(const QVector<smappoint> &points, bool isRoute)
{
    m_area = points;
    m_isRoute = isRoute;
    m_checked = false;                          // ждём areaChecked для новой области
    m_resultHtml.clear();                       // итог прошлой ID04 к новой области не относится

    m_settingText = true;
    ui->textEditCoordinates->setCoordinatesFromMap(points);
    m_settingText = false;

    updateAreaStatus();
    updateSendState();
}

void ReconPage::onAreaCleared()
{
    m_area.clear();
    m_isRoute = false;
    m_checked = false;
    m_resultHtml.clear();
    m_settingText = true;
    ui->textEditCoordinates->clearTextEditContent();
    m_settingText = false;
    updateAreaStatus();
    updateSendState();
}

void ReconPage::onAreaChecked(const AreaCheck &check)
{
    m_check = check;
    m_checked = true;
    updateAreaStatus();
    updateSendState();
}

bool ReconPage::checkMatchesArea() const
{
    if (!m_checked)
        return false;
    const auto lat  = [](const smappoint &p) { return p.latt; };
    const auto lon  = [](const smappoint &p) { return p.longt; };
    const auto glat = [](const GeoPoint &p)  { return p.lat; };
    const auto glon = [](const GeoPoint &p)  { return p.lon; };
    return samePoints(m_area, m_check.vertices, lat, lon, glat, glon);
}

void ReconPage::simplifyArea()
{
    if (m_isRoute || m_area.size() <= CoreConst::AreaMaxVertices)
        return;
    const int before = m_area.size();
    const double areaBefore = mapgeo::areaKm2(m_area);
    const QVector<smappoint> pts = mapgeo::simplifyPolygon(m_area, CoreConst::AreaMaxVertices);
    const double areaAfter = mapgeo::areaKm2(pts);
    const double diff = areaBefore > 0 ? std::fabs(areaAfter - areaBefore) / areaBefore * 100.0 : 0.0;

    emit areaEdited(pts);
    NotificationManager::showSuccess(tr("Вершин: %1 → %2, площа змінилась на %3%")
                                         .arg(before).arg(pts.size()).arg(diff, 0, 'f', diff < 1 ? 2 : 1),
                                     tr("Область спрощено"));
}

void ReconPage::updateAreaStatus()
{
    const int n = m_area.size();
    const bool tooMany = !m_isRoute && n > CoreConst::AreaMaxVertices;
    ui->pushButtonSimplify->setVisible(tooMany);

    if (n == 0) {
        ui->labelAreaStatus->setText(colored(kMuted, tr("Намалюйте область на карті або введіть координати вершин")));
        return;
    }
    if (tooMany) {
        // Рекомендация, не запрет: рисовать можно сколько угодно, но логер примет только 3–20
        ui->labelAreaStatus->setText(colored(kWarn,
            tr("Вершин: %1 — логер приймає не більше %2. Спростіть область, щоб відправити її.")
                .arg(n).arg(CoreConst::AreaMaxVertices)));
        return;
    }
    if (!checkMatchesArea()) {
        ui->labelAreaStatus->setText(colored(kMuted, tr("Перевірка області…")));
        return;
    }
    if (!m_check.ok) {
        ui->labelAreaStatus->setText(colored(kError, QStringLiteral("✕ ") + CoreTexts::areaError(m_check)));
        return;
    }
    ui->labelAreaStatus->setText(colored(kOk, tr("✓ Область коректна · вершин: %1 · %2 км²")
                                                  .arg(n).arg(mapgeo::areaKm2(m_area), 0, 'f', 2)));
}

// ------------------------------------------------------------------
//  Отправка ID04
// ------------------------------------------------------------------

void ReconPage::onAllowedCommands(const QList<quint8> &ids)
{
    m_allowed = ids;
    updateSendState();
}

void ReconPage::onLinkStatus(const LinkStatus &status)
{
    const bool changed = status.state != m_link.state || status.commandPending != m_link.commandPending;
    m_link = status;
    if (changed)
        updateSendState();
}

void ReconPage::onLoggerStatus(const LoggerStatus &status)
{
    m_logger = status;
}

void ReconPage::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    updateSendState();
    updateFlightUi();
}

void ReconPage::updateSendState()
{
    // Первая причина, по которой отправить нельзя, — текстом под кнопкой
    QString reason;
    if (m_readOnly)
        reason = tr("Відкрито минулий політ — лише перегляд");
    else if (m_link.state == LinkState::Closed)
        reason = tr("Радіомодем не підключено (Налаштування → Радіомодем)");
    else if (m_area.isEmpty())
        reason = tr("Задайте область розвідки");
    else if (!m_isRoute && m_area.size() > CoreConst::AreaMaxVertices)
        reason = tr("Забагато вершин — спростіть область");
    else if (!checkMatchesArea())
        reason = tr("Очікується перевірка області");
    else if (!m_check.ok)
        reason = tr("Область не пройшла перевірку");
    else if (!m_allowed.contains(quint8(4)))
        reason = tr("ID04 зараз недоступна в поточному режимі логера");
    else if (m_link.commandPending || m_pendingSeq != 0)
        reason = tr("Очікується відповідь на попередню команду");

    ui->pushButtonSend->setEnabled(reason.isEmpty());
    ui->pushButtonSend->setToolTip(reason);

    // Под кнопкой: итог последней ID04 и почему сейчас нельзя отправить
    QStringList lines;
    if (m_pendingSeq != 0 || m_sending)
        lines << colored(kMuted, tr("Відправлено, очікується відповідь…"));
    else if (!m_resultHtml.isEmpty())
        lines << m_resultHtml;
    if (!reason.isEmpty() && m_pendingSeq == 0 && !m_sending)
        lines << colored(kMuted, reason);
    ui->labelSendStatus->setText(lines.join(QStringLiteral("<br>")));
}

void ReconPage::send()
{
    // Перед отправкой — та же проверка, что у кнопки (на случай гонки сигналов)
    updateSendState();
    if (!ui->pushButtonSend->isEnabled())
        return;

    if (m_logger.spectrumStage == SpectrumStage::Collecting) {
        const auto answer = QMessageBox::question(this, tr("Набір спектра"),
                                                  tr("Набір спектра буде перервано. Продовжити?"));
        if (answer != QMessageBox::Yes)
            return;
    }

    const ReconParams p = params();
    CommandRequest r;
    r.id = 4;
    for (const smappoint &pt : std::as_const(m_area)) {
        GeoPoint g;
        g.lat = pt.latt;
        g.lon = pt.longt;
        r.vertices.append(g);
    }
    r.reconMode = p.reconMode;
    r.intervalS = quint16(p.intervalS);
    r.distanceM = quint16(p.distanceM);
    r.dupFactor = quint8(p.dupFactor);
    r.spectrumS = quint16(p.spectrumS);

    m_sending = true;
    m_early.clear();
    updateSendState();

    // Полёт ещё не начат оператором — начать его с введённым именем перед ID04
    if (!(m_hasSession && m_ownSession)) {
        m_requestedName = flightName();
        emit startFlightRequested(m_requestedName);
    }
    emit sendRequested(r);      // MainWindow: seq = core->sendCommand(r); onCommandSent(seq)
}

void ReconPage::onCommandSent(quint32 seq)
{
    m_sending = false;
    m_pendingSeq = seq;
    // Итог мог прийти раньше номера (заглушка ядра работает в потоке интерфейса)
    const QVector<CommandOutcome> early = std::exchange(m_early, {});
    for (const CommandOutcome &o : early)
        if (o.request.seq == seq) {
            showOutcome(o);
            return;
        }
    updateSendState();
}

void ReconPage::onCommandFinished(const CommandOutcome &outcome)
{
    if (m_sending && outcome.request.id == 4) {
        m_early.append(outcome);
        return;
    }
    if (m_pendingSeq == 0 || outcome.request.seq != m_pendingSeq)
        return;
    showOutcome(outcome);
}

void ReconPage::showOutcome(const CommandOutcome &outcome)
{
    m_pendingSeq = 0;

    const QString time = (outcome.finishTime.isValid() ? outcome.finishTime : QDateTime::currentDateTime())
                             .toLocalTime().toString("HH:mm:ss");
    const QString text = CoreTexts::outcome(outcome);
    const bool ok = CoreTexts::isSuccess(outcome);
    m_resultHtml = colored(ok ? kOk : kError, QStringLiteral("%1 · %2").arg(time, text));

    if (ok)
        NotificationManager::showSuccess(tr("Логер прийняв область і параметри"), tr("ID04"));
    else
        NotificationManager::showError(text, tr("ID04 не виконано"));
    updateSendState();
}

// ------------------------------------------------------------------
//  Полёт (Р2-2)
// ------------------------------------------------------------------

QString ReconPage::flightName() const
{
    const QString name = ui->lineEditFlightName->text().simplified();
    const QString task = ui->lineEditFlightTask->text().simplified();
    QString out = name.isEmpty() ? tr("Політ %1").arg(QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm"))
                                 : name;
    if (!task.isEmpty())
        out += QStringLiteral(" · ") + task;
    return out;
}

void ReconPage::onSessionStarted(const SessionInfo &info)
{
    m_session = info;
    m_hasSession = true;
    // Начат нами — имя совпадает с запрошенным; иначе ядро начало запись само (по первым данным)
    m_ownSession = !m_requestedName.isEmpty() && info.name == m_requestedName;
    if (m_ownSession) {
        ui->lineEditFlightName->clear();
        ui->lineEditFlightTask->clear();
    }
    updateFlightUi();
}

void ReconPage::onSessionFinished(const SessionInfo &info)
{
    if (m_hasSession && info.id != m_session.id)
        return;
    m_hasSession = false;
    m_ownSession = false;
    m_requestedName.clear();
    updateFlightUi();
}

void ReconPage::updateFlightUi()
{
    const bool own = m_hasSession && m_ownSession;
    // Полёт оператора идёт — поля имени не нужны: новые ID04 пойдут в этот же полёт
    ui->labelFlightName->setVisible(!own);
    ui->lineEditFlightName->setVisible(!own);
    ui->labelFlightTask->setVisible(!own);
    ui->lineEditFlightTask->setVisible(!own);
    ui->pushButtonFinishFlight->setVisible(m_hasSession);
    ui->pushButtonFinishFlight->setEnabled(!m_readOnly);

    const QString since = m_session.startTime.isValid()
        ? m_session.startTime.toLocalTime().toString("HH:mm") : QString();
    if (own)
        ui->labelFlightState->setText(tr("Поточний політ: <b>%1</b> · з %2. Нові області підуть у цей політ.")
                                          .arg(m_session.name.toHtmlEscaped(), since));
    else if (m_hasSession)
        ui->labelFlightState->setText(tr("Зараз дані пишуться в автоматичний запис «%1». "
                                         "Новий політ почнеться разом із відправкою області.")
                                          .arg(m_session.name.toHtmlEscaped()));
    else
        ui->labelFlightState->setText(tr("Політ почнеться разом із відправкою області на дрон"));
}
