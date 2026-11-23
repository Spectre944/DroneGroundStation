#ifndef SD_IMPORTER_H
#define SD_IMPORTER_H

/**
    @file  sd_importer.h
    @brief Обработка SD-карты после полёта (Ф13): чтение файлов, сверка с эфиром,
           дополнение базы тем, чего в эфире не было, отчёт.

    Файлы: ГГММДД/ЧЧММСС.CSV — один файл на одну область (ID04).
    Строка: ID в HEX, дальше байты данных через запятую, конец CR LF; Length и CRC нет.
    На SD пишутся ID19, ID20, ID22, ID25, ID28 и итоговый спектр ID23 (2070 байт:
    22 байта как в ID27 + 1024 канала).

    @note  Основа — загрузка SD и сверка в имитаторе, без окон.
           Формат — «Протокол обмена для наземного ПО», п. 10–11.
    @note  Дата — из имени папки ГГММДД (UTC); время назад внутри файла — полночь, дата +1.
    @note  Живёт в потоке ядра, в CoreEngine; пишет в базу через SessionStore.
*/

#include <QObject>
#include <QDate>
#include <functional>
#include <QStringList>
#include <QVector>

#include "core_types.h"

class SessionStore;

class SdImporter : public QObject
{
    Q_OBJECT

public:
    explicit SdImporter(SessionStore *store, QObject *parent = nullptr);

    /**
        @brief Импорт и сверка файлов SD.
        @param files Файлы полёта (порядок не важен — сортируются по папке и имени).
        @note  Данные добавляются в текущий полёт; если его нет — в последний.
               Ход — sdProgress, итог — imported.
    */
    SdReport importFiles(const QStringList &files);

    /**
        @brief Обработка измерения с SD перед записью (пересчёт на 1 м, флаги) — как у эфира.
        @note  Не задана — измерение пишется как есть.
    */
    void setProcessor(std::function<Measurement(const Measurement &)> f) { m_processor = std::move(f); }

signals:
    void progress(int percent);                 ///< ход импорта 0–100
    void imported(const SdReport &report);      ///< импорт окончен

private:
    /**
        @brief Строка SD → пакет.
        @return false — строка ошибочная (не HEX, неизвестный ID, неверная длина).
    */
    static bool parseLine(const QByteArray &line, Packet &packet, QString &error);

    /// Дата из имени папки ГГММДД; пустая — папка названа иначе
    static QDate folderDate(const QString &file);

    /// Время пакета ЧЧ:ММ:СС для ID19, ID20, ID22; пустое — у пакета времени нет
    static QTime packetTime(const Packet &p);

    void addMissing(const Packet &p);           ///< пакет «только на SD» — в базу с источником SD

    SessionStore *m_store = nullptr;
    std::function<Measurement(const Measurement &)> m_processor;
};

#endif // SD_IMPORTER_H
