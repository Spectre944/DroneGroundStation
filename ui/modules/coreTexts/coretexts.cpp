#include "coretexts.h"

#include <QCoreApplication>

namespace {
QString tr(const char *text) { return QCoreApplication::translate("CoreTexts", text); }
}

namespace CoreTexts {

QString rejectReason(RejectReason r)
{
    switch (r) {
    case RejectReason::None:         return QString();
    case RejectReason::PortClosed:   return tr("порт закрито");
    case RejectReason::Busy:         return tr("очікується відповідь на попередню команду");
    case RejectReason::NotAllowed:   return tr("заборонено в поточному режимі / радіомовчання");
    case RejectReason::ReadOnly:     return tr("відкрито минулий політ");
    case RejectReason::BadId:        return tr("невідома команда");
    case RejectReason::AreaNotValid: return tr("область не пройшла перевірку");
    case RejectReason::BadReconMode: return tr("неприпустимий режим розвідки");
    case RejectReason::BadInterval:  return tr("інтервал має бути більше 0");
    case RejectReason::BadDistance:  return tr("відстань має бути більше 0");
    case RejectReason::BadDupFactor: return tr("коефіцієнт дублювання 2–5");
    case RejectReason::BadValue:     return tr("неприпустиме значення");
    }
    return QString();
}

QString outcome(const CommandOutcome &o)
{
    switch (o.result) {
    case CommandResult::Done:      return tr("Виконано");
    case CommandResult::BadPacket: return tr("Помилка пакета (ID02)");
    case CommandResult::Forbidden: return tr("Заборонено в поточному режимі (ID03)");
    case CommandResult::NoAnswer:  return tr("Немає відповіді");
    case CommandResult::Rejected:  return tr("Не відправлено: %1").arg(rejectReason(o.reason));
    }
    return QString();
}

QString areaError(const AreaCheck &check)
{
    if (check.ok)
        return QString();
    const int v = check.vertex + 1;     // для оператора вершины нумеруются с 1
    switch (check.error) {
    case AreaError::None:                return QString();
    case AreaError::TooFewVertices:      return tr("Замало вершин (потрібно 3–20)");
    case AreaError::TooManyVertices:     return tr("Забагато вершин (потрібно 3–20)");
    case AreaError::BadCoordinate:       return tr("Неприпустима координата вершини %1").arg(v);
    case AreaError::DuplicateVertex:     return tr("Вершина %1 збігається з іншою (ближче 1 м)").arg(v);
    case AreaError::Collinear:           return tr("Вершина %1 лежить на одній прямій із сусідніми").arg(v);
    case AreaError::SelfIntersection:    return tr("Сторони області перетинаються");
    case AreaError::NotConvex:           return tr("Область неопукла (вершина %1) — логер помилятиметься «всередині / зовні»").arg(v);
    case AreaError::TooLarge:            return tr("Область більша за допустимий розмір (налаштування)");
    case AreaError::CrossesAntimeridian: return tr("Область перетинає 180-й меридіан");
    case AreaError::RouteNotSupported:   return tr("Маршрут (лінія) не підтримується — потрібен багатокутник");
    }
    return QString();
}

} // namespace CoreTexts
