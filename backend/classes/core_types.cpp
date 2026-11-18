#include "core_types.h"

/**
    @brief Регистрирует все типы core_types.h в системе типов Qt.
    @note  Имя в кавычках нужно, чтобы тип находился и по имени
           (сигналы, объявленные через строки, QVariant, отладка).
*/
void registerCoreTypes()
{
    qRegisterMetaType<GeoPoint>("GeoPoint");
    qRegisterMetaType<DataSource>("DataSource");
    qRegisterMetaType<Packet>("Packet");
    qRegisterMetaType<LinkState>("LinkState");
    qRegisterMetaType<LinkStatus>("LinkStatus");
    qRegisterMetaType<LoggerMode>("LoggerMode");
    qRegisterMetaType<ReconMode>("ReconMode");
    qRegisterMetaType<SpectrumStage>("SpectrumStage");
    qRegisterMetaType<LoggerDevice>("LoggerDevice");
    qRegisterMetaType<DeviceCheck>("DeviceCheck");
    qRegisterMetaType<DeviceState>("DeviceState");
    qRegisterMetaType<LoggerStatus>("LoggerStatus");
    qRegisterMetaType<RoutePoint>("RoutePoint");
    qRegisterMetaType<Measurement>("Measurement");
    qRegisterMetaType<SpectrumStopReason>("SpectrumStopReason");
    qRegisterMetaType<SpectrumPortion>("SpectrumPortion");
    qRegisterMetaType<SpectrumInfo>("SpectrumInfo");
    qRegisterMetaType<Spectrum>("Spectrum");
    qRegisterMetaType<CommandRequest>("CommandRequest");
    qRegisterMetaType<CommandResult>("CommandResult");
    qRegisterMetaType<RejectReason>("RejectReason");
    qRegisterMetaType<CommandOutcome>("CommandOutcome");
    qRegisterMetaType<EventCode>("EventCode");
    qRegisterMetaType<CoreEvent>("CoreEvent");
    qRegisterMetaType<AlarmType>("AlarmType");
    qRegisterMetaType<AlarmLevel>("AlarmLevel");
    qRegisterMetaType<Alarm>("Alarm");
    qRegisterMetaType<AreaError>("AreaError");
    qRegisterMetaType<AreaCheck>("AreaCheck");
    qRegisterMetaType<GridData>("GridData");
    qRegisterMetaType<CoreSettings>("CoreSettings");
    qRegisterMetaType<SessionInfo>("SessionInfo");
    qRegisterMetaType<SessionData>("SessionData");
    qRegisterMetaType<SessionFilter>("SessionFilter");
    qRegisterMetaType<ArchivePoint>("ArchivePoint");
    qRegisterMetaType<ArchiveResult>("ArchiveResult");
    qRegisterMetaType<SdReport>("SdReport");
}
