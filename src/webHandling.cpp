

// #define DEBUG_WIFI(m) SERIAL_DBG.print(m)

#include <Arduino.h>
#include <ArduinoOTA.h>
#if ESP32
#include <WiFi.h>
// #include <esp_wifi.h>
 #else
#include <ESP8266WiFi.h>   

#endif

#include <time.h>
//needed for library
#include <DNSServer.h>

#include <IotWebConf.h>
#include <IotWebConfUsing.h> // This loads aliases for easier class names.
#include <IotWebConfTParameter.h>

#include "common.h"
#include "webHandling.h"
#include "statusHandling.h"

#include <N2kTypes.h>


// -- Method declarations.
void handleRoot();
void convertParams();

// -- Callback methods.
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper*);

DNSServer dnsServer;
WebServer server(80);

bool gParamsChanged = true;

uint16_t gCapacityAh;
uint16_t gChargeEfficiencyPercent;
uint16_t gMinPercent;
uint16_t gTailCurrentmA;
uint16_t gFullVoltagemV;
uint16_t gFullDelayS;
float gShuntResistancemR;
uint16_t gMaxCurrentA;
char gCustomName[64] = "NMEA-Batterymonitor";
tN2kBatType gBatteryType = N2kDCbt_AGM;

// -- We can add a legend to the separator
IotWebConf iotWebConf(gCustomName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

char maxCurrentValue[NUMBER_LEN];
char VoltageCalibrationFactorValue[NUMBER_LEN];
char CurrentCalibrationFactorValue[NUMBER_LEN];
char shuntResistanceValue[NUMBER_LEN];

IotWebConfParameterGroup ShuntGroup = IotWebConfParameterGroup("ShuntGroup","Shunt");
IotWebConfNumberParameter shuntResistance = IotWebConfNumberParameter("Shunt resistance [m&#8486;]", "shuntR", shuntResistanceValue, NUMBER_LEN, "0.750", "0..100", "min='0.001' max='100' step='0.001'");
IotWebConfNumberParameter maxCurrent = IotWebConfNumberParameter("Expected max current [A]", "maxA", maxCurrentValue, NUMBER_LEN, "200", "1..500", "min='1' max='500' step='1'");
IotWebConfNumberParameter VoltageCalibrationFactor = IotWebConfNumberParameter("Voltage calibration factor", "VoltageCalibrationFactor", VoltageCalibrationFactorValue, NUMBER_LEN, "1.0000", "e.g. 1.00001", "step='0.00001'");
IotWebConfNumberParameter CurrentCalibrationFactor = IotWebConfNumberParameter("Current calibration factor", "CurrentCalibrationFactor", CurrentCalibrationFactorValue, NUMBER_LEN, "1.0000", "e.g. 1.00001", "step='0.00001'");

static char BatteryTypeValues[][STRING_LEN] = {
    "0",
    "1",
    "2"
};
static char BatteryTypeNames[][STRING_LEN] = {
    "flooded lead acid",
    "GEL",
    "AGM"
};

char BatteryTypeValue[STRING_LEN];
char battCapacityValue[NUMBER_LEN];
char chargeEfficiencyValue[NUMBER_LEN];
char minSocValue[NUMBER_LEN];
IotWebConfParameterGroup BatteryGroup = IotWebConfParameterGroup("Battery","Battery");
IotWebConfSelectParameter BatteryType = IotWebConfSelectParameter("Type", "BatteryType", BatteryTypeValue, STRING_LEN, (char*)BatteryTypeValues, (char*)BatteryTypeNames, sizeof(BatteryTypeValues) / STRING_LEN, STRING_LEN);
IotWebConfNumberParameter battCapacity = IotWebConfNumberParameter("Capacity [Ah]", "battAh", battCapacityValue, NUMBER_LEN, "100", "1..300", "min='1' max='300' step='1'");
IotWebConfNumberParameter chargeEfficiency = IotWebConfNumberParameter("charge efficiency [%]", "cheff", chargeEfficiencyValue, NUMBER_LEN, "95", "1..100", "min='1' max='100' step='1'");
IotWebConfNumberParameter minSoc = IotWebConfNumberParameter("Minimun SOC [%]", "minsoc", minSocValue, NUMBER_LEN, "10", "1..100", "min='1' max='100' step='1'");

char tailCurrentValue[NUMBER_LEN];
char fullVoltageValue[NUMBER_LEN];
char fullDelayValue[NUMBER_LEN];
IotWebConfParameterGroup fullGroup = IotWebConfParameterGroup("FullD","Battery full detection");
IotWebConfNumberParameter tailCurrent = IotWebConfNumberParameter("Tail current [A]", "tailC", tailCurrentValue, NUMBER_LEN, "1.500", "0..100", "min='0.001' max='100' step='0.001'");
IotWebConfNumberParameter fullVoltage = IotWebConfNumberParameter("Voltage when full [V]", "fullV", fullVoltageValue, NUMBER_LEN, "12.85", "0..38", "min='0.01' max='38' step='0.01'");
IotWebConfNumberParameter fullDelay = IotWebConfNumberParameter("Delay before full [s]", "fullDelay", fullDelayValue, NUMBER_LEN, "60", "1..7200", "min='1' max='7200' step='1'");

void wifiStoreConfig() {
    iotWebConf.saveConfig();
}

void wifiConnected(){
   ArduinoOTA.begin();
}

void onSetSoc() {
    String soc = server.arg("soc");
    soc.trim();
    if(!soc.isEmpty()) {
        uint16_t socVal = soc.toInt();
        gBattery.setBatterySoc(((float)socVal)/100.0);
        //Serial.printf("Set soc to %.2f",gBattery.soc());
    }

    server.send(200, "text/html", SOC_RESPONSE);
} 

void handleSetRuntime() {

    String page = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>{v}</title>";
    page.replace("{v}", "Battery monitor");
    page += "<style>";
    page += ".de{background-color:#ffaaaa;} .em{font-size:0.8em;color:#bb0000;padding-bottom:0px;} .c{text-align: center;} div,input,select{padding:5px;font-size:1em;} input{width:95%;} select{width:100%} input[type=checkbox]{width:auto;scale:1.5;margin:10px;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#16A1E7;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} fieldset{border-radius:0.3rem;margin: 0px;}";
    // page.replace("center", "left");
    page += "</style>";
    page += "</head><body>";
    page += SOC_FORM;

    page += "<table border=0 align=center>";
    page += "<tr><td align=left>Go to <a href = 'config'>configure page</a> to change configuration.</td></tr>";
    page += "<tr><td align=left>Go to <a href='/'>main page</a>.</td></tr>";
    page += "</table>";
    page += "</body> </html>";

    page += "</body></html>\n";

  server.send(200, "text/html", page);
}

void wifiSetup() {
    ShuntGroup.addItem(&shuntResistance);
    ShuntGroup.addItem(&maxCurrent);
    ShuntGroup.addItem(&VoltageCalibrationFactor);
    ShuntGroup.addItem(&CurrentCalibrationFactor);

    BatteryGroup.addItem(&BatteryType);
    BatteryGroup.addItem(&battCapacity);
    BatteryGroup.addItem(&chargeEfficiency);
    BatteryGroup.addItem(&minSoc);

    fullGroup.addItem(&fullVoltage);
    fullGroup.addItem(&tailCurrent);
    fullGroup.addItem(&fullDelay);
     
    iotWebConf.setStatusPin(STATUS_PIN,ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);

    iotWebConf.addParameterGroup(&ShuntGroup);
    iotWebConf.addParameterGroup(&BatteryGroup);
    iotWebConf.addParameterGroup(&fullGroup);
  
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.setFormValidator(formValidator);
    iotWebConf.getApTimeoutParameter()->visible = true;

    // -- Initializing the configuration.
    iotWebConf.init();
  
    convertParams();

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/config", [] { iotWebConf.handleConfig(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });
  
    server.on("/setruntime", handleSetRuntime);
    server.on("/setsoc",HTTP_POST,onSetSoc);
}

void wifiLoop() {
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();
  ArduinoOTA.handle();
}

void handleRoot() {
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal())
    {
    // -- Captive portal request were already served.
    return;
    }

    String page = HTML_Start_Doc;
    page.replace("{v}", iotWebConf.getThingName());


    page += "<style>";
    page += ".de{background-color:#ffaaaa;} .em{font-size:0.8em;color:#bb0000;padding-bottom:0px;} .c{text-align: center;} div,input,select{padding:5px;font-size:1em;} input{width:95%;} select{width:100%} input[type=checkbox]{width:auto;scale:1.5;margin:10px;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#16A1E7;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} fieldset{border-radius:0.3rem;margin: 0px;}";
    // page.replace("center", "left");
    page += "</style>";
    page += "<meta http-equiv=refresh content=15 />";
    page += HTML_Start_Body;
    page += "<table border=0 align=center>";
    page += "<tr><td>";

    page += HTML_Start_Fieldset;
    page += HTML_Fieldset_Legend;
    page.replace("{l}", "Battery status");

        page += HTML_Start_Table;
        if (gSensorInitialized) {
            page += "<tr><td align=left>Battery Voltage:</td><td>" + String(gBattery.voltage(), 2) + "V" + "</td></tr>";
            page += "<tr><td align=left>Shunt current:</td><td>" + String(gBattery.current(), 2) + "A" + "</td></tr>";
            page += "<tr><td align=left>Avg consumption:</td><td>" + String(gBattery.averageCurrent(), 2) + "A" + "</td></tr>";
            page += "<tr><td align=left>Battery soc:</td><td>" + String(gBattery.soc(), 2) + "</td></tr>";

            uint16_t tTgh = gBattery.tTg() / 3600;

            page += "<tr><td align=left>Time to go:</td><td>" + String(tTgh) + "h" + "</td></tr>";;
            page += "<tr><td align=left>Battery full:</td><td>" + String(gBattery.isFull() ? "true" : "false") + "</td></tr>";
            page += "<tr><td align=left>Temperature:</td><td>" + String(gBattery.temperatur()) +"�C" + "</td></tr>";
        }
        else {
            page += "<tr><td><div><font color=red size=+1><b>Sensor failure!</b></font></div></td></tr>";
        }
        page += HTML_End_Table;

        page += HTML_End_Fieldset;

        page += HTML_Start_Fieldset;
        page += HTML_Fieldset_Legend;
        page.replace("{l}", "Shunt configuration");

        page += "<table border=0 align=center width=100%>";
        page += "<tr><td align=left>Shunt resistance:</td><td>" + String(gShuntResistancemR, 3) + "m&#8486;</td></tr>";
        page += "<tr><td align=left>Shunt max current:</td><td>" + String(gMaxCurrentA) + "A</td></tr>";

    page += HTML_End_Table;
    page += HTML_End_Fieldset;

    page += HTML_Start_Fieldset;
    page += HTML_Fieldset_Legend;
    page.replace("{l}", "Battery configuration");
  
        page += "<table border=0 align=center width=100%>";
        page += "<tr><td align=left>Type:</td><td>" + String(BatteryTypeNames[gBatteryType]) + "</td></tr>";
        page += "<tr><td align=left>Capacity:</td><td>" + String(gCapacityAh) + "Ah</td></tr>";
        page += "<tr><td align=left>Efficiency:</td><td>" + String(gChargeEfficiencyPercent) + "%</td></tr>";
        page += "<tr><td align=left>Min soc:</td><td>" + String(gMinPercent) + "%</td></tr>";

        float TailCurrentA = gTailCurrentmA;
        float FullVoltageV = gFullVoltagemV;
        page += "<tr><td align=left>Tail current:</td><td>" + String((TailCurrentA / 1000), 3) + "A</td></tr>";
        page += "<tr><td align=left>Batt full voltage:</td><td>" + String((FullVoltageV / 1000),2) + "V</td></tr>";

        page += "<tr><td align=left>Batt full delay:</td><td>" + String(gFullDelayS) + "s</td></tr>";

    page += HTML_End_Table;

    page += HTML_End_Fieldset;

    page += "</td></tr>";
    page += HTML_End_Table;

    page += "<br>";
    page += "<br>";

    page += HTML_Start_Table;
    page += "<tr><td align=left>Go to <a href = 'config'>configure page</a> to change configuration.</td></tr>";
    page += "<tr><td align=left>Go to <a href='setruntime'>runtime modification page</a> to change runtime data.</td></tr>";
    page += "<tr><td><font size=1>Version: " + String(Version) + "</font></td></tr>";
    page += HTML_End_Table;
    page += HTML_End_Body;

    page += HTML_End_Doc;

    server.send(200, "text/html", page);
}

void convertParams() {
    gShuntResistancemR = atof(shuntResistanceValue);
    gMaxCurrentA = atoi(maxCurrentValue);
    gCapacityAh = atoi(battCapacityValue);
    gChargeEfficiencyPercent = atoi(chargeEfficiencyValue);
    gMinPercent = atoi(minSocValue);

    uint16_t tCv = atof(tailCurrentValue) * 1000;
    gTailCurrentmA = tCv;
    
    uint16_t fVv = atof(fullVoltageValue) * 1000;
    gFullVoltagemV = fVv;

    gFullDelayS = atoi(fullDelayValue);
    gBatteryType = tN2kBatType(atoi(BatteryTypeValue));
    gVoltageCalibrationFactor = atof(VoltageCalibrationFactorValue);
    gCurrentCalibrationFactor = atof(CurrentCalibrationFactorValue);

}

void configSaved(){ 
  convertParams();
  gParamsChanged = true;
} 

bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper){ 
  bool result = true;

  int l = 0;

  
    if (server.arg(shuntResistance.getId()).toFloat() <=0)
    {
        shuntResistance.errorMessage = "Shunt resistance has to be > 0";
        result = false;
    }

    l = server.arg(maxCurrent.getId()).toInt();
    if ( l <= 0)
    {
        maxCurrent.errorMessage = "Maximal current must be > 0";
        result = false;
    }

    l = server.arg(battCapacity.getId()).toInt();
    if (l <= 0)
    {
        battCapacity.errorMessage = "Battery capacity must be > 0";
        result = false;
    }

    l = server.arg(chargeEfficiency.getId()).toInt();
    if (l <= 0  || l> 100) {
        chargeEfficiency.errorMessage = "Charge efficiency must be between 1% and 100%";
        result = false;
    }


    l = server.arg(minSoc.getId()).toInt();
    if (l <= 0  || l> 100) {
        minSoc.errorMessage = "Minimum SOC must be between 1% and 100%";
        result = false;
    }

    l = server.arg(tailCurrent.getId()).toInt();
    if (l < 0 ) {
        tailCurrent.errorMessage = "Tail current must be > 0";
        result = false;
    }

    l = server.arg(fullVoltage.getId()).toInt();
    if (l < 0 ) {
        fullVoltage.errorMessage = "Voltage when full must be > 0";
        result = false;
    }

    l = server.arg(fullDelay.getId()).toInt();
    if (l < 0 ) {
        fullDelay.errorMessage = "Delay before full must be > 0";
        result = false;
    }

  return result;
  }
