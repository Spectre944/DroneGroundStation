#ifndef SDIMPORT_H
#define SDIMPORT_H

/**
    @file  sdimport.h
    @brief Окно «Імпорт з SD-карти» (Р2-6).
    @note  По образцу окна шифрования базы из РХБ (DatabaseCrypt): область, куда
           перетаскиваются файлы (или клик — выбор файлов), список, кнопка действия.
           Отличия: принимаются несколько файлов и целые папки (ищутся *.CSV во вложенных
           папках ГГММДД), вместо шифрования — импорт ядром:
             importRequested(files) → DroneCore::importSd;
             DroneCore::sdProgress   → onSdProgress (полоса хода);
             DroneCore::sdImported   → onSdImported (отчёт).
           После отчёта — кнопка «Переглянути політ» (openFlightRequested): данные
           добавлены в текущий полёт (нет — в последний), его надо перечитать.
*/

#include <QLabel>
#include <QStringList>
#include <QWidget>

#include "backend/classes/core_types.h"

class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QGridLayout;

/// Область «перетягніть сюди»: файлы *.CSV и папки; клик — выбор файлов
class SdDropArea : public QLabel
{
    Q_OBJECT
public:
    explicit SdDropArea(QWidget *parent = nullptr);

signals:
    void filesDropped(const QStringList &files);    ///< уже развёрнуто: только *.CSV
    void clicked();

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    void applyStyle(bool highlighted);
    bool m_highlighted = false;
    bool m_styling = false;
};

class SdImport : public QWidget
{
    Q_OBJECT
public:
    explicit SdImport(QWidget *parent = nullptr);

    /// Файлы *.CSV из путей: файл — как есть, папка — все *.CSV внутри (с вложенными)
    static QStringList collectCsv(const QStringList &paths);

public slots:
    void onSdProgress(int percent);
    void onSdImported(const SdReport &report);

signals:
    void importRequested(const QStringList &files);
    void openFlightRequested();

private:
    void buildUi();
    void addFiles(const QStringList &files);
    void chooseFiles();
    void chooseFolder();
    void startImport();
    void setRunning(bool running);
    void updateButtons();

    QStringList     m_files;
    bool            m_running = false;
    QString         m_lastDir;

    SdDropArea     *m_drop = nullptr;
    QListWidget    *m_list = nullptr;
    QLabel         *m_listTitle = nullptr;
    QPushButton    *m_folder = nullptr;
    QPushButton    *m_clear = nullptr;
    QPushButton    *m_import = nullptr;
    QProgressBar   *m_progress = nullptr;

    QWidget        *m_reportBox = nullptr;
    QGridLayout    *m_reportGrid = nullptr;
    QPlainTextEdit *m_details = nullptr;
    QPushButton    *m_openFlight = nullptr;
};

#endif // SDIMPORT_H
