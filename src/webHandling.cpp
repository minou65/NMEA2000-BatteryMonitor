

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
bool gSaveParams = false;

uint16_t gCapacityAh;
uint16_t gChargeEfficiencyPercent;
uint16_t gMinPercent;
uint16_t gTailCurrentmA;
uint16_t gFullVoltagemV;
uint16_t gFullDelayS;
float gCurrentThreshold;
float gShuntResistancemR;
uint16_t gMaxCurrentA;
char gCustomName[64] = "NMEA-Batterymonitor";
tN2kBatType gBatteryType = N2kDCbt_AGM;
tN2kBatNomVolt gBatteryVoltage = N2kDCbnv_12v;
tN2kBatChem gBatteryChemistry = N2kDCbc_LeadAcid;


// -- We can add a legend to the separator
IotWebConf iotWebConf(gCustomName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

char InstanceValue[NUMBER_LEN];
char SIDValue[NUMBER_LEN];
char SourceValue[NUMBER_LEN];
iotwebconf::ParameterGroup InstanceGroup = iotwebconf::ParameterGroup("InstanceGroup", "NMEA 2000 Settings");
iotwebconf::NumberParameter InstanceParam = iotwebconf::NumberParameter("Instance", "InstanceParam", InstanceValue, NUMBER_LEN, "1", "1..255", "min='1' max='254' step='1'");
iotwebconf::NumberParameter SIDParam = iotwebconf::NumberParameter("SID", "SIDParam", SIDValue, NUMBER_LEN, "255", "1..254", "min='1' max='255' step='1'");
iotwebconf::NumberParameter SourceParam = iotwebconf::NumberParameter("N2KSource", "N2KSource", SourceValue, NUMBER_LEN, "22", nullptr, nullptr);

char maxCurrentValue[NUMBER_LEN];
char VoltageCalibrationFactorValue[NUMBER_LEN];
char CurrentCalibrationFactorValue[NUMBER_LEN];
char shuntResistanceValue[NUMBER_LEN];

iotwebconf::ParameterGroup ShuntGroup = iotwebconf::ParameterGroup("ShuntGroup","Shunt");
iotwebconf::NumberParameter shuntResistance = iotwebconf::NumberParameter("Shunt resistance [m&#8486;]", "shuntR", shuntResistanceValue, NUMBER_LEN, "0.750", "0..100", "min='0.001' max='100' step='0.001'");
iotwebconf::NumberParameter maxCurrent = iotwebconf::NumberParameter("Expected max current [A]", "maxA", maxCurrentValue, NUMBER_LEN, "200", "1..500", "min='1' max='500' step='1'");
iotwebconf::NumberParameter VoltageCalibrationFactor = iotwebconf::NumberParameter("Voltage calibration factor", "VoltageCalibrationFactor", VoltageCalibrationFactorValue, NUMBER_LEN, "1.0000", "e.g. 1.00001", "step='0.00001'");
iotwebconf::NumberParameter CurrentCalibrationFactor = iotwebconf::NumberParameter("Current calibration factor", "CurrentCalibrationFactor", CurrentCalibrationFactorValue, NUMBER_LEN, "1.0000", "e.g. 1.00001", "step='0.00001'");

char BatTypeValue[STRING_LEN];
char BatNomVoltValue[STRING_LEN];
char BatChemValue[STRING_LEN];
char battCapacityValue[NUMBER_LEN];
char chargeEfficiencyValue[NUMBER_LEN];
char minSocValue[NUMBER_LEN];
char BatteryReplacmentDateValue[DATE_LEN];
char BatteryReplacmentTimeValue[TIME_LEN];
char BatteryManufacturerValue[STRING_LEN];
iotwebconf::ParameterGroup BatteryGroup = iotwebconf::ParameterGroup("Battery","Battery");
iotwebconf::SelectParameter BatType        = iotwebconf::SelectParameter("Type", "BatType", BatTypeValue, STRING_LEN, (char*)BatTypeValues, (char*)BatTypeNames, sizeof(BatTypeValues) / STRING_LEN, STRING_LEN, "2");
iotwebconf::SelectParameter BatNomVoltType = iotwebconf::SelectParameter("Voltage", "BatNomVoltType", BatNomVoltValue, STRING_LEN, (char*)BatNomVoltValues, (char*)BatNomVoltNames, sizeof(BatNomVoltValues) / STRING_LEN, STRING_LEN, "1");
iotwebconf::SelectParameter BatChemType    = iotwebconf::SelectParameter("Chemistrie", "BatChemType", BatChemValue, STRING_LEN, (char*)BatChemValues, (char*)BatChemNames, sizeof(BatChemValues) / STRING_LEN, STRING_LEN, "0");



iotwebconf::NumberParameter battCapacity = iotwebconf::NumberParameter("Capacity [Ah]", "battAh", battCapacityValue, NUMBER_LEN, "100", "1..300", "min='1' max='300' step='1'");
iotwebconf::NumberParameter chargeEfficiency = iotwebconf::NumberParameter("charge efficiency [%]", "cheff", chargeEfficiencyValue, NUMBER_LEN, "95", "1..100", "min='1' max='100' step='1'");
iotwebconf::NumberParameter minSoc = iotwebconf::NumberParameter("Minimun SOC [%]", "minsoc", minSocValue, NUMBER_LEN, "10", "1..100", "min='1' max='100' step='1'");
iotwebconf::DateParameter BatteryReplacmentDateParameter = iotwebconf::DateParameter("Replacment date", "BattDate", BatteryReplacmentDateValue, DATE_LEN, "2024-01-01");
iotwebconf::TextParameter BatterymanufacturerParameter = iotwebconf::TextParameter("Manufacturer", "BattManufacturer", BatteryManufacturerValue, STRING_LEN);
// TimeParameter BatteryReplacmentTimeParameter = TimeParameter("Replacment time", "BattTime", BatteryReplacmentTimeValue, TIME_LEN, "22:00");


char tailCurrentValue[NUMBER_LEN];
char fullVoltageValue[NUMBER_LEN];
char fullDelayValue[NUMBER_LEN];
char CurrentThresholdValue[NUMBER_LEN];
iotwebconf::ParameterGroup fullGroup = iotwebconf::ParameterGroup("FullD","Battery full detection");
iotwebconf::NumberParameter tailCurrent = iotwebconf::NumberParameter("Tail current [A]", "tailC", tailCurrentValue, NUMBER_LEN, "1.500", "0..65", "min='0.001' max='65' step='0.001'");
iotwebconf::NumberParameter fullVoltage = iotwebconf::NumberParameter("Voltage when full [V]", "fullV", fullVoltageValue, NUMBER_LEN, "12.85", "0..38", "min='0.01' max='38' step='0.01'");
iotwebconf::NumberParameter fullDelay = iotwebconf::NumberParameter("Delay before full [s]", "fullDelay", fullDelayValue, NUMBER_LEN, "60", "1..7200", "min='1' max='7200' step='1'");
iotwebconf::NumberParameter CurrentThreshold = iotwebconf::NumberParameter("Current threshold [A]", "CurrentThreshold", CurrentThresholdValue, NUMBER_LEN, "0.10", "0.00..2.00", "min='0.00' max='2.00' step='0.01'");


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
        float socVal = soc.toInt() / 100.00;
        gBattery.setBatterySoc((float)socVal);
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

    InstanceGroup.addItem(&InstanceParam);
    InstanceGroup.addItem(&SIDParam);
    iotWebConf.addHiddenParameter(&SourceParam);
    iotWebConf.addParameterGroup(&InstanceGroup);

    ShuntGroup.addItem(&shuntResistance);
    ShuntGroup.addItem(&maxCurrent);
    ShuntGroup.addItem(&VoltageCalibrationFactor);
    ShuntGroup.addItem(&CurrentCalibrationFactor);

    BatteryGroup.addItem(&BatType);
    BatteryGroup.addItem(&BatNomVoltType);
    BatteryGroup.addItem(&BatChemType);
    BatteryGroup.addItem(&battCapacity);
    BatteryGroup.addItem(&chargeEfficiency);
    BatteryGroup.addItem(&minSoc);
    BatteryGroup.addItem(&BatteryReplacmentDateParameter);
    // BatteryGroup.addItem(&BatteryReplacmentTimeParameter);
    BatteryGroup.addItem(&BatterymanufacturerParameter);

    fullGroup.addItem(&fullVoltage);
    fullGroup.addItem(&tailCurrent);
    fullGroup.addItem(&fullDelay);
    fullGroup.addItem(&CurrentThreshold);
     
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

  if (gSaveParams) {
      Serial.println(F("Parameters are changed,save them"));

      String s;

      s = (String)gN2KSource;
      strncpy(SourceParam.valueBuffer, s.c_str(), NUMBER_LEN);

      iotWebConf.saveConfig();
      gSaveParams = false;
  }
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
            page += "<tr><td align=left>Avg consumption:</td><td>" + String(gBattery.averageCurrent() * -1, 2) + "A" + "</td></tr>";
            page += "<tr><td align=left>State of charge:</td><td>" + String(gBattery.soc() * 100, 2) + "%</td></tr>";

            if (gBattery.tTg() != INFINITY) {
                uint16_t tTgh = gBattery.tTg() / 3600;
                page += "<tr><td align=left>Time to go:</td><td>" + String(tTgh) + "h" + "</td></tr>";
            }
            else {
                page += "<tr><td align=left>Time to go:</td><td> INFINITY </td></tr>";
            }

            page += "<tr><td align=left>Battery full:</td><td>" + String(gBattery.isFull() ? "true" : "false") + "</td></tr>";
            page += "<tr><td align=left>Temperature:</td><td>" + String(gBattery.temperatur()) +"&deg;C" + "</td></tr>";
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
        page += "<tr><td align=left>Type:</td><td>" + String(BatTypeNames[gBatteryType]) + "</td></tr>";
        page += "<tr><td align=left>Capacity:</td><td>" + String(gCapacityAh) + "Ah</td></tr>";
        page += "<tr><td align=left>Efficiency:</td><td>" + String(gChargeEfficiencyPercent) + "%</td></tr>";
        page += "<tr><td align=left>Min SOC:</td><td>" + String(gMinPercent) + "%</td></tr>";

        float TailCurrentA = gTailCurrentmA;
        float FullVoltageV = gFullVoltagemV;
        page += "<tr><td align=left>Tail current:</td><td>" + String((TailCurrentA / 1000), 3) + "A</td></tr>";
        page += "<tr><td align=left>Full voltage:</td><td>" + String((FullVoltageV / 1000),2) + "V</td></tr>";
        page += "<tr><td align=left>Full delay:</td><td>" + String(gFullDelayS) + "s</td></tr>";

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
    gMaxCurrentA =atoi(maxCurrentValue); 
    gCapacityAh = atoi(battCapacityValue);
    gChargeEfficiencyPercent = atoi(chargeEfficiencyValue);
    gMinPercent = atoi(minSocValue);

    uint16_t tCv = static_cast<uint16_t>(atof(tailCurrentValue) * 1000);
    gTailCurrentmA = tCv;
    
    uint16_t fVv = static_cast<uint16_t>(atof(fullVoltageValue) * 1000);
    gFullVoltagemV = fVv;

    gFullDelayS = atoi(fullDelayValue);
    gBatteryType = tN2kBatType(atoi(BatTypeValue));
    gBatteryVoltage = tN2kBatNomVolt(atoi(BatNomVoltValue));
    gBatteryChemistry = tN2kBatChem(atoi(BatChemValue));

    gVoltageCalibrationFactor = atof(VoltageCalibrationFactorValue);
    gCurrentCalibrationFactor = atof(CurrentCalibrationFactorValue);
    gCurrentThreshold = atof(CurrentThresholdValue);

    gN2KInstance = atoi(InstanceValue);
    gN2KSID = atoi(SIDValue);
    gN2KSource = atoi(SourceValue);
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
