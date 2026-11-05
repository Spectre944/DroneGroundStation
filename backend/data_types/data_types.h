#ifndef DATA_TYPES_H
#define DATA_TYPES_H


#include <QString>
#include <QList>
#include <QPair>
#include <QMetaType>


typedef QPair<float, float> CoordPairF;
Q_DECLARE_METATYPE(CoordPairF)

typedef QPair<double, double> CoordPairD;
// Q_DECLARE_METATYPE(CoordPairD)

Q_DECLARE_METATYPE(QVector<float>)
Q_DECLARE_METATYPE(QVector<bool>)

/**
    @brief Макс допустимое число ошибок прибора
*/
#define MAX_NUM_ERRORS 3


/**
    @brief Распределение приборов в зависимости от типа проекта
    проект "ДЖУРА"            -     LCD-4 , BDBG , MKSU-M ,   AirMar , GPS-721U
    проект "КАШАЛОТ"("БРДМ")  -     LCD-4 , SVNG , SRHR   ,   AirMar

*/
/* Imitator
namespace PORT_NAME {
    static constexpr char AirMarPort[]  = "COM23";
    static constexpr char GpsPort[]     = "COM24";
    static constexpr char LcdPort[]     = "COM22";
    static constexpr char BdbgPort[]    = "COM21";
    static constexpr char MKSUPort[]    = "COM20";

    static constexpr char SvngPort[]    = "COM21";
    static constexpr char SrhrPort[]    = "COM20";
}
*/
// /* MOXA
namespace PORT_NAME {
    static constexpr char AirMarPort[]  = "COM13";
    static constexpr char GpsPort[]     = "COM14";
    static constexpr char LcdPort[]     = "COM11";
    static constexpr char BdbgPort[]    = "COM12";
    static constexpr char MKSUPort[]    = "COM12";

    static constexpr char SvngPort[]    = "COM21";
    static constexpr char SrhrPort[]    = "COM20";
}
// */

/* MOXA + GPS Imitator
namespace PORT_NAME {
    static constexpr char AirMarPort[]  = "COM13";
    static constexpr char GpsPort[]     = "COM24";
    static constexpr char LcdPort[]     = "COM12";
    static constexpr char BdbgPort[]    = "COM11";
    static constexpr char MKSUPort[]    = "COM11";

    static constexpr char SvngPort[]    = "COM21";
    static constexpr char SrhrPort[]    = "COM20";
}
*/

/**
    @brief Структуры данных и диагностики для приборов проектов "Джура", "Кашалот(БРДМ)"
 */
//////////////////////////////// AirMar ( Джура , Кашалот )  //////////////////////



/**
    @brief Значения bool флагов структур
    @note  true     - все ОК
           false    - проблема
*/
struct airmarData {

    airmarData(){
        clear();
    }
    airmarData(const airmarData&) = default;
    airmarData& operator=(const airmarData&) = default;

    bool failureFlag;
    float windSpeed;   
    int windDirect;
    bool validWindDirect;
    int windDirectCorrect;
    float temperature;
    int humidity;
    int pressure;
    float dewPoint;
    int magnCompass;
    bool  coordFlag;
    CoordPairD coord;

    void clear() {
        failureFlag = false;
        windSpeed = 0.0f;
        windDirect = 0;
        validWindDirect = false;
        windDirectCorrect = 0;
        temperature = 0.0f;
        humidity = 0;
        pressure = 0;
        dewPoint = 0.0f;
        magnCompass = 0;
        coordFlag = false;
        coord = CoordPairD(0.0, 0.0);
    }
};

struct airmarTest {

    airmarTest(){
        clear();
    }
    airmarTest(const airmarTest&) = default;
    airmarTest& operator=(const airmarTest&) = default;

    bool failureFlag;
    bool noRespFlag;
    bool noCorrectDataFlag;

    void clear() {
        failureFlag = false;
        noRespFlag = false;
        noCorrectDataFlag = false;
    }

    /**
        @brief Значения bool флагов
        @note  true     - все ОК
               false    - проблема
    */
};

Q_DECLARE_METATYPE(airmarData)
Q_DECLARE_METATYPE(airmarTest)

//////////////////////////////// GPS-721U( Джура  )  ////////////////////////////

struct gpsData {

    gpsData(){
        clear();
    }
    gpsData(const gpsData&) = default;
    gpsData& operator=(const gpsData&) = default;

    bool failureFlag;
    bool  coordFlag;
    CoordPairD coord;

    void clear() {
        failureFlag = false;
        coordFlag = false;
        coord = CoordPairD(0.0, 0.0);
    }
};

struct gpsTest {

    gpsTest(){
        clear();
    }
    gpsTest(const gpsTest&) = default;
    gpsTest& operator=(const gpsTest&) = default;

    bool failureFlag;
    bool noRespFlag;
    bool noCorrectDataFlag;

    void clear() {
        failureFlag = false;
        noRespFlag = false;
        noCorrectDataFlag = false;
    }
};

Q_DECLARE_METATYPE(gpsData)
Q_DECLARE_METATYPE(gpsTest)

///////////////////////// LCD - 4  ( Кашалот, Джура ) ////////////////////////////////////

struct lcdData {

    lcdData(){
        clear();
    }
    lcdData(const lcdData&) = default;
    lcdData& operator=(const lcdData&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    QVector<int> typeAgent;
    QVector<float> conc;
    QVector<float> dose;
    QVector<int> Bars;
    QVector<int> hazLevel;
    QVector<int> BPeak;
    int max_posit;

    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        typeAgent.clear();
        conc.clear();
        dose.clear();
        Bars.clear();
        hazLevel.clear();
        BPeak.clear();
        max_posit = -1;
    }
};

struct lcdTest {

    lcdTest(){
        clear();
    }
    lcdTest(const lcdTest&) = default;
    lcdTest& operator=(const lcdTest&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    bool StreamActive;
    QString sysStatus;
    QString operMode;
    QString powerStatus;
    int sieveLife;
    QStringList warnFlags;
    QStringList majorFault;
    QStringList faultFlags;



    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        StreamActive = false;
        sysStatus.clear();
        operMode.clear();
        sieveLife = 0;
        warnFlags.clear();
        majorFault.clear();
        faultFlags.clear();
        powerStatus.clear();
    }
};

Q_DECLARE_METATYPE(lcdData)
Q_DECLARE_METATYPE(lcdTest)

///////////////////////// BDBG    ( Джура) ////////////////////////////////////


/**
    @brief BDBG - 2 прибора установленные на машине.
           Размер QVector - 2
*/

struct bdbgData {
    bdbgData(){
        clear();
    }
    bdbgData(const bdbgData&) = default;
    bdbgData& operator=(const bdbgData&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    QVector<float> PED;
    QVector<int> statisticError;
    float maxPed;
    bool  bdbgDerThreat;

    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        PED.clear();
        statisticError.clear();
        maxPed = 0.0f;
        bdbgDerThreat = false;
    }
};


struct bdbgTest {
    bdbgTest(){
        clear();
    }
    bdbgTest(const bdbgTest&) = default;
    bdbgTest& operator=(const bdbgTest&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    bool noCorrectDataFlag;
    QString infoBdbg;

    QVector<bool> highFreqDetectFailure;
    QVector<bool> lowFreqDetectFailure;
    QVector<bool> falsityData;
    QVector<bool> noRespDevFlag;
    QVector<bool> rangeExceeded;

    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        noCorrectDataFlag = false;
        infoBdbg.clear();
        highFreqDetectFailure.clear();
        lowFreqDetectFailure.clear();
        falsityData.clear();
        noRespDevFlag.clear();
        rangeExceeded.clear();
    }
};

Q_DECLARE_METATYPE(bdbgData)
Q_DECLARE_METATYPE(bdbgTest)

///////////////////////// SVNG-T  ( Кашалот) ////////////////////////////////////


struct svngData {    
    svngData(){
        clear();
    }
    svngData(const svngData&) = default;
    svngData& operator=(const svngData&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    int radAngle;
    float maxPed;
    QVector<float> PED;
    QVector<int> statError;
    bool  svngDerThreat;


    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        radAngle = -1;
        maxPed = 0.0f;
        PED.clear();
        statError.clear();
        svngDerThreat = false;
    }
};

struct svngTest {
    svngTest(){
        clear();
    }
    svngTest(const svngTest&) = default;
    svngTest& operator=(const svngTest&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    bool noRespFlag;
    bool noCorrectDataFlag;
    QVector<bool> highSensorError;
    QVector<bool> lowSensorError;
    QVector<bool> validDataSensor;
    QVector<bool> connectSensor;
    QString devInfo;

    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        noRespFlag = false;
        noCorrectDataFlag = false;
        highSensorError.clear();
        lowSensorError.clear();
        validDataSensor.clear();
        connectSensor.clear();
        devInfo.clear();
    }
};

Q_DECLARE_METATYPE(svngData)
Q_DECLARE_METATYPE(svngTest)

///////////////////////// SRHR ( Кашалот ) ////////////////////////////////////

struct srhrData {

    srhrData(){
        clear();
    }
    srhrData(const srhrData&) = default;
    srhrData& operator=(const srhrData&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    float PED;
    int statisticError;
    bool flagAtom;
    bool alertFlagBio;
    bool alertFlagCh;
    float concCh;
    bool alertFlagArs;
    float concArs;
    bool alertFlagHno;
    float concHno;
    bool alertFlagPhs;
    float concPhs;
    bool alertFlagS;
    float concS;

    bool  srhrDerThreat;
    bool  srhrHNOThreat;
    bool  srhrAsThreat;
    bool  srhrSThreat;
    bool  srhrPThreat;
    bool  srhrChThreat;

    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        PED  = 0.0f;
        statisticError = 0;
        flagAtom= false;
        alertFlagBio = false;
        alertFlagCh= false;
        concCh  = 0.0f;
        alertFlagArs = false;
        concArs  = 0.0f;
        alertFlagHno = false;
        concHno  = 0.0f;
        alertFlagPhs = false;
        concPhs   = 0.0f;
        alertFlagS = false;
        concS  = 0.0f;        
        srhrDerThreat = false;
        srhrHNOThreat = false;
        srhrAsThreat = false;
        srhrSThreat = false;
        srhrPThreat = false;
        srhrChThreat = false;
    }
};


struct srhrTest {

    srhrTest(){
        clear();
    }
    srhrTest(const srhrTest&) = default;
    srhrTest& operator=(const srhrTest&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    bool noRespFlag;
    bool noCorrectDataFlag;
    bool readiDev;
    bool waitFlag;
    bool failureRadFlag;
    bool lowSensorError;
    bool highSensorError;
    bool lowPowerFlag;
    bool failureChemFlag;
    bool serviceChemFlag;
    bool h2Flag;

    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        noRespFlag = false;
        noCorrectDataFlag = false;
        readiDev = false;
        waitFlag = false;
        failureRadFlag = false;
        lowSensorError  = false;
        highSensorError = false;
        lowPowerFlag = false;
        failureChemFlag  = false;
        serviceChemFlag = false;
        h2Flag  = false;
    }
};

Q_DECLARE_METATYPE(srhrData)
Q_DECLARE_METATYPE(srhrTest)


//////////////////////////////// MKSU-M  ( Джура  )  ////////////////////////////

struct mksuData {

    mksuData(){
        clear();
    }
    mksuData(const mksuData&) = default;
    mksuData& operator=(const mksuData&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    int timeStamp;
    int accumDoseTime;
    int statisticError;
    float Dose;
    float PED;
    QString dataType;
    bool  mksuDerThreat;
    bool  mksuDoseThreat;

    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        timeStamp = 0;        
        accumDoseTime = 0;
        statisticError = 0;
        Dose = 0.0f;
        PED = 0.0f;
        dataType.clear();
        mksuDerThreat  = false;
        mksuDoseThreat  = false;
    }
};

struct mksuTest {

    mksuTest(){
        clear();
    }
    mksuTest(const mksuTest&) = default;
    mksuTest& operator=(const mksuTest&) = default;

    bool failureFlag;
    bool partialFailureFlag;
    bool noRespFlag;
    bool noCorrectDataFlag;
    bool exceedthreshDose;
    bool exceedthreshPreDose;
    bool exceedthreshPed;
    bool builtInDetectFailure;
    bool overflowPed;
    bool exceedOneZvHour;
    bool highFreqGammaDetectFailure;
    bool lowFreqGammaDetectFailure;
    bool betaDetectFailure;
    bool alphaDetectFailure;
    bool exceedGammaBackgroundMeasurtBeta;

    void clear() {
        failureFlag = false;
        partialFailureFlag = false;
        noRespFlag = false;
        noCorrectDataFlag = false;
        exceedthreshDose = false;
        exceedthreshPreDose = false;
        exceedthreshPed = false;
        builtInDetectFailure = false;
        overflowPed = false;
        exceedOneZvHour = false;
        highFreqGammaDetectFailure = false;
        lowFreqGammaDetectFailure = false;
        betaDetectFailure = false;
        alphaDetectFailure = false;
        exceedGammaBackgroundMeasurtBeta = false;
    }
};

Q_DECLARE_METATYPE(mksuData)
Q_DECLARE_METATYPE(mksuTest)


////////////////////////////////////    ТИПЫ ПРИБОРОВ   /////////////////////////////////////////////////////////

enum class TypeDev {
    /**
        @brief Типы приборов для проектов "Джура", "Кашалот(БРДМ)"
    */
    AIRMAR,
    GPS,
    LCD,
    BDBG,
    SVNG,
    SRHR,
    MKSU
};

Q_DECLARE_METATYPE(TypeDev)

//////////////////////////////////////// МАССИВЫ ЗАПРОСОВ К ПРИБОРАМ ///////////////////////////////////////////////////////


namespace AIRMAR {
    /**
        @brief Список контейнеров(QByteArray) запросов данных AIRMAR и время ожидания ответа
        @note  Список состоит только из одного пустого QByteArray()  с возможностью расширения
    */
    const QList<QByteArray> REQ_LIST = {
        QByteArray()
    };
    const int wait_time = 3000;
}

namespace GPS {
    /**
        @brief Список контейнеров(QByteArray) запросов данных GPS и время ожидания ответа
        @note  Список состоит только из одного пустого QByteArray()  с возможностью расширения
    */
    const QList<QByteArray> REQ_LIST = {
        QByteArray()
    };
    const int wait_time = 3000;
}

    namespace LCD {
    /**
        @brief Список контейнеров (QByteArray) запроса данных LCD-4
        @note  Список состоит из двух команд - пустой контейнер и контейнер запроса данных
    */
    const QList<QByteArray> REQ_LIST = {
        QByteArray(),
        QByteArray::fromHex("00001A0003001900FFFF")
    };

    const int wait_time = 2000;

    /**
        @brief Max время запуска потока
    */
    const int MaxRespTime = 20'000;

    const int MinNumBytes = 5;

    /**
        @brief Размер полного пакета LCD
    */
    const int FullPackSize = 406;


    /**
        @brief Позиция первого блока угроз
    */
    const int FirstThreatPos = 154;


    /**
        @brief Позиция последнего блока угроз
    */
    const int LastThreatPos = 234;

    const int ThreatBlockSize = 16;


}


namespace BDBG {
    /**
        @brief Список контейнеров (QByteArray) запроса данных ПЕД БДБГ
        @note  Список состоит из одной команды запроса данных с возможностью расширения
    */
    const QList<QByteArray> REQ_LIST = {
        QByteArray::fromHex("55AA01E1E2")};

    const int wait_time = 1000;
}

namespace SVNG {
    /**
        @brief Список контейнеров (QByteArray) запроса данных ПЕД СВНГ
        @note  Список состоит из одной команды запроса данных с возможностью расширения
    */
    const QList<QByteArray> REQ_LIST = {
        QByteArray::fromHex("55AA01061118")
    };

    const int wait_time = 200;

    const int DataBlockSize = 30;

    const int FirstBlockPos = 5;

    const int LastBlockPos = 23;

    const int BlockSize = 6;

    const float minPedLevel = 0.3f;

}


namespace SRHR {
    /**
        @brief Список контейнеров(QByteArray) запросов данных SRHR
        @note  Список состоит из трех команд запросов данных с возможностью расширения
    */
    const QList<QByteArray> REQ_LIST = {
        /**
            @brief Запрос данных радиационного и химического каналов
        */
        QByteArray::fromHex("55AA0106848B"),

        /**
            @brief Запрос порогового уровня сигнализации канала R  PC - Dev
        */
        QByteArray::fromHex("55AA01068188"),

        /**
            @brief Запрос изменения порогового уровня сигнализации канала R
        */
        QByteArray::fromHex("55AA010A81000000008B")
    };

    const int wait_time = 200;
}




namespace MKSU {
    /**
        @brief Список контейнеров (QByteArray) запроса данных  МКСУ
        @note  Список состоит из одной команды запроса данных с возможностью расширения
    */
    const QList<QByteArray> REQ_LIST = {
        QByteArray::fromHex("55AA01F2F3")
    };

    const int DataBlockSize = 27;

    const int wait_time = 1000;
}

namespace PROGRAMM_SETTINGS{

constexpr int RECON_MODE_COUNT = 4;


    enum class ReconMode {
        WAIT = 0,
        AREA,
        ROUTE,
        DATA_EXCHANGE
    };
#ifdef DZURA
    constexpr int SOUND_MODE_COUNT = 2;
#endif

#ifdef BDRM
    constexpr int SOUND_MODE_COUNT = 3;
#endif


    enum class SoundMode {
        OFF = 0,
        COMPUTER,
        EXTERNAL_SYSTEM
    };
}

Q_DECLARE_METATYPE(PROGRAMM_SETTINGS::ReconMode);
Q_DECLARE_METATYPE(PROGRAMM_SETTINGS::SoundMode);




namespace movingCar {

    /**
        @brief Порог в метрах (машина "движется", если смещение > 20 м)
    */
    const double MOVEMENT_THRESHOLD   = 20.0;

    /**
        @brief Минимальное кол-во точек для анализа (на случай, если буфер ещё не заполнен)
    */
    const int MIN_POINTS_FOR_ANALYSIS = 3;

    /**
        @brief Максимальное кол-во точек для анализа (размер PollDispatcher::checkMoveList)
    */
    const int MAX_POINTS = 7;
}


namespace typeThreats {

    const quint8 RAD_THREAT = 0b00000001;

    const quint8 CHEM_THREAT = 0b00000010;

    const quint8 BIOL_THREAT = 0b00000100;
}


/**
    @brief Режим опроса приборов
*/
enum class reqDevMode {
    Auto,
    Handle
};


/**
    @brief Параметр стабильности воздуха
*/
enum class AR {
    U,    // нестабильный
    N,    // нейтральный
    S     // стабильный
};

/**
    @brief Параметр видимости атаки противника
*/
enum class attackStatus {
    Visible,
    NotVisible,
    Default
};



/**
    @brief Структура начальных данных разведки
*/
struct startReconParams {    

    startReconParams(){
        clear();
    }
    startReconParams(const startReconParams&) = default;
    startReconParams& operator=(const startReconParams&) = default;

    PROGRAMM_SETTINGS::ReconMode recon_mode;
    reqDevMode req_dev_mode;
    int duration_reconn;            // продолжительность разведки - выставляем для контроля непревышения времени
    QString coord_package;          // пакет координат области, маршрута
    bool isCoordinatesValid;        // валидность набора введенных координат
    int coordinatesCount;           // кол-во координат в пакете
    // AR air_resist;               // стабильность воздуха
    int release_zone;               // радиус зоны генерации угрозы
    int danger_distance;            // опасная дистанция
    attackStatus attack_status;     // статус атаки

    void clear() {
        recon_mode = PROGRAMM_SETTINGS::ReconMode::WAIT;
        req_dev_mode = reqDevMode::Auto;
        duration_reconn = 0;
        coord_package.clear();
        isCoordinatesValid = false;
        coordinatesCount = 0;
        // air_resist = AR::U;
        release_zone = 0;
        danger_distance = 0;
        attack_status = attackStatus::Default;
    }
};

Q_DECLARE_METATYPE(startReconParams)
Q_DECLARE_METATYPE(reqDevMode)
Q_DECLARE_METATYPE(AR)
Q_DECLARE_METATYPE(attackStatus)











#endif // DATA_TYPES_H
