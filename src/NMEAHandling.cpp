#include "common.h"
#include "NMEAHandling.h"
#include "statusHandling.h"
#include "BatteryConfig.h"
#include "NMEAConfig.h"

#include <esp_mac.h>

#include <N2kMessages.h>
#include <NMEA2000_CAN.h>

tN2kSyncScheduler DCBatStatusScheduler(false, 1500, 500);
tN2kSyncScheduler DCStatusScheduler(false, 1500, 510);
tN2kSyncScheduler BatConfScheduler(false, 5000, 520); // Non periodic


int8_t BatTemperatureCoefficient = 53;
double BatPeukertExponent = 1.251;

bool TimeSet = false;
unsigned long LastTimeUpdate = 0;

// Function prototypes for NMEA2000 message handlers
void ParseN2kSystemTime(const tN2kMsg& N2kMsg);
void ParseN2kGNSS(const tN2kMsg& N2kMsg);

// NMEA2000 message handlers
typedef struct {
    unsigned long PGN;
    void (*Handler)(const tN2kMsg& N2kMsg);
} tNMEA2000Handler;

tNMEA2000Handler NMEA2000Handlers[] = {
    {126992L, ParseN2kSystemTime},  // System Time
    {129029L, ParseN2kGNSS},        // GNSS Position Data
    {0, 0}
};

// List of messages to receive
const unsigned long ReceiveMessages[] PROGMEM = {
    126992L,  // System Time
    129029L,  // GNSS Position Data
    0
};


// List here messages your device will transmit.
const unsigned long TransmitMessages[] = { 
	127506L,  // DC Detailed Status
	127508L,  // Battery Status
	127513L,  // DC Battery Configuration
    0 };

// ============================================================================
// TIME SYNCHRONIZATION FUNCTIONS
// ============================================================================

/**
 * Set ESP32 system time from NMEA2000 time
 */
void SetSystemTime(uint16_t DaysSince1970, double SecondsSinceMidnight) {
    if (DaysSince1970 == 0xFFFF || SecondsSinceMidnight > 86400) {
        return;
    }

    // Update only every 10 minutes (except first time)
    if (TimeSet && (millis() - LastTimeUpdate < 600000)) {
        return;
    }

    time_t timestamp = (DaysSince1970 * 86400UL) + (time_t)SecondsSinceMidnight;

    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    TimeSet = true;
    LastTimeUpdate = millis();

    char timeStr[64];
    struct tm* timeinfo = localtime(&timestamp);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    Serial.printf("System time set from NMEA2000: %s UTC\n", timeStr);
}

/**
 * Parse PGN 126992 - System Time
 */
void ParseN2kSystemTime(const tN2kMsg& N2kMsg) {
    unsigned char SID;
    uint16_t SystemDate;
    double SystemTime;
    tN2kTimeSource TimeSource;

    if (ParseN2kPGN126992(N2kMsg, SID, SystemDate, SystemTime, TimeSource)) {
        if (TimeSource == N2ktimes_GPS || TimeSource == N2ktimes_GLONASS) {
            SetSystemTime(SystemDate, SystemTime);

            if (debugMode) {
                Serial.printf("Received System Time (PGN 126992): Date=%u, Time=%.1fs, Source=%d\n",
                    SystemDate, SystemTime, TimeSource);
            }
        }
    }
}

/**
 * Parse PGN 129029 - GNSS Position Data (contains time)
 */
void ParseN2kGNSS(const tN2kMsg& N2kMsg) {
    unsigned char SID;
    uint16_t DaysSince1970;
    double SecondsSinceMidnight;
    double Latitude;
    double Longitude;
    double Altitude;
    tN2kGNSStype GNSStype;
    tN2kGNSSmethod GNSSmethod;
    unsigned char nSatellites;
    double HDOP;
    double PDOP;
    double GeoidalSeparation;
    unsigned char nReferenceStations;
    tN2kGNSStype ReferenceStationType;
    uint16_t ReferenceSationID;
    double AgeOfCorrection;

    if (ParseN2kPGN129029(N2kMsg, SID, DaysSince1970, SecondsSinceMidnight,
        Latitude, Longitude, Altitude, GNSStype, GNSSmethod,
        nSatellites, HDOP, PDOP, GeoidalSeparation,
        nReferenceStations, ReferenceStationType,
        ReferenceSationID, AgeOfCorrection)) {

        SetSystemTime(DaysSince1970, SecondsSinceMidnight);

        if (debugMode) {
            Serial.printf("Received GNSS Time (PGN 129029): Date=%u, Time=%.1fs, Sats=%u\n",
                DaysSince1970, SecondsSinceMidnight, nSatellites);
        }
    }
}

/**
 * Handle incoming NMEA2000 messages
 */
void handleN2kMessages(const tN2kMsg& N2kMsg) {
    for (uint8_t i = 0; NMEA2000Handlers[i].PGN != 0; i++) {
        if (N2kMsg.PGN == NMEA2000Handlers[i].PGN) {
            NMEA2000Handlers[i].Handler(N2kMsg);
            break;
        }
    }
}

void OnN2kOpen() {
    // Start schedulers now.
    DCBatStatusScheduler.UpdateNextTime();
    DCStatusScheduler.UpdateNextTime();
    BatConfScheduler.UpdateNextTime();
}

void CheckN2kSourceAddressChange() {
    uint8_t SourceAddress_ = NMEA2000.GetN2kSource();

    if (SourceAddress_ != nmeaConfig.Source()) {
#ifdef DEBUG_MSG
        Serial.printf("Address Change: New Address=%d\n", SourceAddress_);
#endif // DEBUG_MSG
        nmeaConfig.SetSource(SourceAddress_);
        SaveParams = true;
    }
}

void N2kInit() {
    uint8_t chipid[6];
    uint32_t id = 0;
    int i = 0;

    // Generate unique number from chip id
	esp_efuse_mac_get_default(chipid);
    for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

    // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
    NMEA2000.SetN2kCANMsgBufSize(8);
    NMEA2000.SetN2kCANReceiveFrameBufSize(150);
    NMEA2000.SetN2kCANSendFrameBufSize(150);


    // Set Product information
    NMEA2000.SetProductInformation(
        "1", // Manufacturer's Model serial code
        100, // Manufacturer's product code
        "BatteryMonitor",  // Manufacturer's Model ID
        Version,  // Manufacturer's Software version code
        Version // Manufacturer's Model version
    );

    NMEA2000.SetDeviceInformation(
        id,      // Unique number. Use e.g. Serial number.
        170,    // Device function=Battery. See codes on https://web.archive.org/web/20190531120557/https://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
        35,     // Device class=Electrical Generation. See codes on  https://web.archive.org/web/20190531120557/https://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
        2046    // Just choosen free from code list on https://web.archive.org/web/20190529161431/http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
    );

    NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, nmeaConfig.Source());

    NMEA2000.ExtendTransmitMessages(TransmitMessages);
    NMEA2000.ExtendReceiveMessages(ReceiveMessages);

	NMEA2000.SetConfigurationInformation("", CustomName, "");

    NMEA2000.SetOnOpen(OnN2kOpen);
    NMEA2000.Open();
}

void SendN2kBattery(void) {
    tN2kMsg N2kMsg_;

    const Statistics& stats_ = batteryStatus.statistics();

    unsigned int BatteryTimeToGo_ = batteryStatus.tTg();
    float BatteryCurrent_ = batteryStatus.current();
    float BatteryVoltage_ = batteryStatus.voltage();
    double BatteryAvgConsumption_ = batteryStatus.averageCurrent();
    double BatteryPower_ = roundf(batteryStatus.current() * batteryStatus.voltage());
    double BatteryTemperature_ = batteryStatus.temperatur();
    double BatteryConsumedAs_ = roundf(stats_.consumedAs / 3.6);
    double BatterySOC_ = batteryStatus.soc();
    byte BatterIsFull_ = batteryStatus.isFull();
    
    if (DCBatStatusScheduler.IsTime()) {
        DCBatStatusScheduler.UpdateNextTime();
        SetN2kDCBatStatus(
            N2kMsg_, 
            nmeaConfig.Instance(), 
            BatteryVoltage_, 
            BatteryCurrent_, 
            CToKelvin(BatteryTemperature_), 
            nmeaConfig.SID());
        NMEA2000.SendMsg(N2kMsg_);
    }

    if (DCStatusScheduler.IsTime()) {
        DCStatusScheduler.UpdateNextTime();
        SetN2kDCStatus(
            N2kMsg_, 
            nmeaConfig.SID(), 
            nmeaConfig.Instance(), 
            N2kDCt_Battery, 
            static_cast<unsigned char>(BatterySOC_ * 100), 
            0, 
            BatteryTimeToGo_, 
            BatteryVoltage_, 
            AhToCoulomb(batteryConfig.Capacity_Ah()));

        NMEA2000.SendMsg(N2kMsg_);
    }

    if (BatConfScheduler.IsTime()) {
        BatConfScheduler.UpdateNextTime();
        SetN2kBatConf(
            N2kMsg_, 
            nmeaConfig.Instance(), 
            batteryConfig.Type(),
            N2kDCES_Yes, 
            batteryConfig.NomVolt(), 
            batteryConfig.Chemistry(), 
            AhToCoulomb(batteryConfig.Capacity_Ah()), 
            BatTemperatureCoefficient, 
            BatPeukertExponent, 
            batteryConfig.ChargeEfficiency_Percent());
        NMEA2000.SendMsg(N2kMsg_);
    }
}

void N2Kloop() {

    SendN2kBattery();
    NMEA2000.ParseMessages();

    CheckN2kSourceAddressChange();

    if (TimeSet && (millis() - LastTimeUpdate > 3600000)) {
        Serial.println("Warning: No time update received for 1 hour");
        TimeSet = false;
    }

    // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
    if (Serial.available()) {
        Serial.read();
    }
}