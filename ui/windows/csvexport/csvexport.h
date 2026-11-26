#ifndef CSVEXPORT_H
#define CSVEXPORT_H

/**
    @file  csvexport.h
    @brief Окно «Експорт у CSV» (Р2-7): выбор полёта (или всех), частей и папки.
    @note  Открывается из меню «Бази даних → Експорт у CSV…» и из ПКМ по полёту
           в «База даних» (тогда полёт уже выбран).
           Окно само к ядру не обращается:
             - список полётов — сигнал flightsRequested → DroneCore::listSessions → onSessionsListed;
             - экспорт — exportRequested(id, папка, части) → DroneCore::exportCsv → onExportFinished.
           Имена файлов задаёт ядро (CsvExporter::fileBase): flight_<id>_<назва>_measurements.csv и т. д.
           «Усі польоти» — по одному: следующий полёт уходит после ответа на предыдущий
           (в DroneCore::exportFinished нет номера полёта).
           Под списком — выбранный полёт (или все) в том же виде, что в таблице «База даних».
           Последняя папка запоминается в настройках (csv/lastFolder).
*/

#include <QWidget>

#include "backend/classes/core_types.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class StickyTableModel;
class StickyTableView;

class CsvExportWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CsvExportWindow(QWidget *parent = nullptr);

    void open(qint64 sessionId = -1);   ///< показать окно; id > 0 — выбрать этот полёт
    bool isBusy() const { return !m_queue.isEmpty() || m_waiting; }
    static QString defaultFolder();     ///< <папка программы>/export

public slots:
    void onSessionsListed(const QVector<SessionInfo> &list);
    void onExportFinished(bool ok, const QString &path);
    void setCurrentSession(qint64 id);  ///< текущий полёт — помечается «поточний»

signals:
    void flightsRequested();                                            ///< → DroneCore::listSessions
    void exportRequested(qint64 sessionId, const QString &folder, quint32 parts);  ///< → DroneCore::exportCsv

private:
    void fillCombo();
    void updatePreview();
    void updateState();
    void startExport();
    void sendNext();
    void finish();
    quint32 parts() const;

    QComboBox    *m_flight = nullptr;
    StickyTableModel *m_model = nullptr;
    StickyTableView  *m_view = nullptr;
    QCheckBox    *m_meas = nullptr;
    QCheckBox    *m_track = nullptr;
    QCheckBox    *m_spectra = nullptr;
    QCheckBox    *m_events = nullptr;
    QLineEdit    *m_folder = nullptr;
    QPushButton  *m_browse = nullptr;
    QPushButton  *m_openFolder = nullptr;
    QPushButton  *m_export = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel       *m_status = nullptr;

    QVector<SessionInfo> m_flights;
    qint64  m_select = -1;              ///< выбрать после прихода списка
    qint64  m_currentSession = -1;
    bool    m_waitList = false;

    // Экспорт: очередь полётов
    QVector<SessionInfo> m_queue;
    SessionInfo m_sending;
    bool    m_waiting = false;          ///< ждём exportFinished
    int     m_total = 0;
    int     m_done = 0;
    QStringList m_errors;
    QString m_target;                   ///< папка этого экспорта
};

#endif // CSVEXPORT_H
