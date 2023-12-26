


#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to D5 
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to D4

#include <N2kMessages.h>
#include <NMEA2000_CAN.h>

#include "NMEAHandling.h"

#include "common.h"
#include "statusHandling.h"

// Set time offsets
#define SlowDataUpdatePeriod 5000  // Time between CAN Messages sent
#define TempSendOffset 0

uint8_t gN2KSource = 22;
char gBatteryInstance = '1';
char gBatterySID = '1';


// List here messages your device will transmit.
const unsigned long TransmitMessages[] = { 127506L,127508L,127513L,0 };


int8_t BatTemperatureCoefficient = 53;
double PeukertExponent = 1.251;

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

    // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
    NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, gN2KSource);

    // Here we tell library, which PGNs we transmit
    NMEA2000.ExtendTransmitMessages(TransmitMessages);


    NMEA2000.Open();
}

bool IsTimeToUpdate(unsigned long NextUpdate) {
    return (NextUpdate < millis());
}

unsigned long InitNextUpdate(unsigned long Period, unsigned long Offset = 0) {
    return millis() + Period + Offset;
}

void SetNextUpdate(unsigned long& NextUpdate, unsigned long Period) {
    while (NextUpdate < millis()) NextUpdate += Period;
}

void SendN2kBattery(void) {
    static unsigned long SlowDataUpdated = InitNextUpdate(SlowDataUpdatePeriod, TempSendOffset);
    tN2kMsg N2kMsg;


    if (IsTimeToUpdate(SlowDataUpdated)) {
        SetNextUpdate(SlowDataUpdated, SlowDataUpdatePeriod);

        const Statistics& stats = gBattery.statistics();

        double BatteryTimeToGo = -1;
        double BatteryCurrent = gBattery.current();
        double BatteryVoltage = gBattery.voltage();
        double BatteryAvgConsumption = gBattery.averageCurrent();
        double BatteryPower = roundf(gBattery.current() * gBattery.voltage());
        double BatteryTemperature = gBattery.temperatur();
        double BatteryConsumedAs = roundf(stats.consumedAs / 3.6);
        double BatterySOC = gBattery.soc();
        byte BatterIsFull = gBattery.isFull();

        if (gBattery.tTg() != INFINITY) {
            BatteryTimeToGo = roundf(gBattery.tTg() / 60);
        }
        SetN2kDCBatStatus(N2kMsg, gBatteryInstance, BatteryVoltage, BatteryCurrent, BatteryTemperature, gBatterySID);
        NMEA2000.SendMsg(N2kMsg);

        SetN2kDCStatus(N2kMsg, gBatterySID, gBatteryInstance, N2kDCt_Battery, BatterySOC, BatteryTimeToGo, N2kDoubleNA, N2kDoubleNA);
        NMEA2000.SendMsg(N2kMsg);

        SetN2kBatConf(N2kMsg, gBatteryInstance, N2kDCbt_Gel, N2kDCES_Yes, N2kDCbnv_12v, N2kDCbc_LeadAcid, AhToCoulomb(gCapacityAh), BatTemperatureCoefficient, PeukertExponent, gChargeEfficiencyPercent);
        NMEA2000.SendMsg(N2kMsg);
    }

}

void N2Kloop() {

    SendN2kBattery();
    NMEA2000.ParseMessages();

    // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
    if (Serial.available()) {
        Serial.read();
    }
}