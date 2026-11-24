#ifndef CORETEXTS_H
#define CORETEXTS_H

/**
    @file  coretexts.h
    @brief Тексты для оператора по ответам ядра: итог команды и ошибка проверки области.
    @note  Таблицы 6.1 и 6.2 из «Наземное ПО — задания Р2 (Никита)». Один источник
           текстов для страницы «Розвідка», панели команд (Р2-4) и т. п.
           Все строки — через tr() (контекст «CoreTexts»).
*/

#include <QString>
#include "backend/classes/core_types.h"

namespace CoreTexts {

/**
    @brief Итог команды (таблица 6.1): «Виконано», «Немає відповіді»,
           «Не відправлено: порт закрито» …
*/
QString outcome(const CommandOutcome &o);

/// Причина, по которой команда не отправлена (без «Не відправлено: »)
QString rejectReason(RejectReason r);

/**
    @brief Ошибка проверки области (таблица 6.2). Номер вершины — с 1.
    @return пустая строка, если check.ok
*/
QString areaError(const AreaCheck &check);

/// true — итог «хороший» (Done), для цвета текста
inline bool isSuccess(const CommandOutcome &o) { return o.result == CommandResult::Done; }

} // namespace CoreTexts

#endif // CORETEXTS_H
