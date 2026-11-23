#ifndef CSV_EXPORTER_H
#define CSV_EXPORTER_H

/**
    @file  csv_exporter.h
    @brief Экспорт полёта в CSV (Ф14) — для Excel и передачи другим службам.

    Файлы в выбранной папке (<полёт> — номер и имя полёта):
      <полёт>_measurements.csv      — измерения, одна строка на измерение;
      <полёт>_track.csv             — маршрут;
      <полёт>_spectrum_<номер>.csv  — по файлу на спектр: сведения и 1024 канала;
      <полёт>_events.csv            — события, команды, тревоги по времени.

    Формат — чтобы файл сразу правильно открывался в Excel с украинскими настройками:
      UTF-8 с BOM; разделитель полей «;»; десятичный разделитель — из настроек
      (запятая для Excel, точка для других программ); заголовки — на языке
      из настроек (uk / en).

    @note  Заголовки и слова в таблицах — здесь, на двух языках: файлы перевода
           ядру не нужны.
*/

#include <QString>
#include <QStringList>

#include "core_types.h"

class CsvExporter
{
public:
    /**
        @brief  Выгрузить полёт.
        @param  data     Все данные полёта (SessionStore::sessionDataFor).
        @param  folder   Папка для файлов; нет — создаётся.
        @param  parts    Что выгружать — флаги ExportPart.
        @param  settings Десятичный разделитель и язык.
        @return true — все файлы записаны; список записанных — files().
    */
    bool exportSession(const SessionData &data, const QString &folder, quint32 parts,
                       const CoreSettings &settings);

    QStringList files() const { return m_files; }   ///< записанные файлы
    QString error() const { return m_error; }       ///< причина, если не удалось

    /// Имя полёта для файлов: номер и имя без символов, недопустимых в Windows
    static QString fileBase(const SessionInfo &info);

private:
    bool writeFile(const QString &path, const QStringList &rows);
    QString num(double v, int decimals) const;              ///< число с нужным десятичным разделителем
    QString time(const QDateTime &t) const;                 ///< дата и время UTC
    QString tr(const char *uk, const char *en) const { return m_en ? QString::fromUtf8(en) : QString::fromUtf8(uk); }
    static QString field(const QString &s);                 ///< поле в кавычках, если в нём «;», кавычки или перевод строки

    QStringList measurementRows(const SessionData &d) const;
    QStringList trackRows(const SessionData &d) const;
    QStringList spectrumRows(const Spectrum &s) const;
    QStringList eventRows(const SessionData &d) const;

    QChar       m_dec = QLatin1Char(',');
    bool        m_en = false;
    QStringList m_files;
    QString     m_error;
};

#endif // CSV_EXPORTER_H
