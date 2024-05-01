

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
#include <DNSServer.h>
#include <IotWebConf.h>
#include <IotWebConfESP32HTTPUpdateServer.h>

#include "common.h"
#include "webHandling.h"
#include "statusHandling.h"
#include "IotWebRoot.h"

#include <N2kTypes.h>

#define SOC_RESPONSE \
"<!DOCTYPE HTML>\
    <html> <head>\
            <meta charset=\"UTF-8\">\
            <meta http-equiv=\"refresh\" content=\"3; url=/\">\
            <script type=\"text/javascript\">\
                window.location.href = \"/\"\
            </script>\
            <title>SOC set</title>\
        </head>\
        <body>\
            Soc has been updated <br><a href='/'>Back</a>\
        </body>\
    </html>\n"

#define SOC_FORM \
"\
<table border=0 align=center>\
    <tr><td>\
        <form align=left action=\"/setsoc\" method=\"POST\">\
            <fieldset style=\"border: 1px solid\">\
                <legend>Set state of charge</legend>\
                <label for=\"soc\">New battery soc</label><br>\
                <input name=\"soc\" id=\"soc\" value=\"100\" /><br><br>\
                <button type=\"submit\">Set</button>\
            </fieldset>\
        </form>\
    </td></td>\
</table>\
"

// -- Method declarations.
void handleData();
void handleRoot();
void convertParams();

// -- Callback methods.
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper*);

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;

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
NMEAConfig Config = NMEAConfig();

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

class CustomHtmlFormatProvider : public iotwebconf::HtmlFormatProvider {
protected:
    virtual String getFormEnd() {
        String _s = HtmlFormatProvider::getFormEnd();
        _s += F("</br>Return to <a href='/'>home page</a>.");
        return _s;
    }
};
CustomHtmlFormatProvider customHtmlFormatProvider;

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
    Serial.begin(115200);
    Serial.println();
    Serial.println("starting up...");

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
    iotWebConf.setHtmlFormatProvider(&customHtmlFormatProvider);

    iotWebConf.addParameterGroup(&Config);
    iotWebConf.addParameterGroup(&ShuntGroup);
    iotWebConf.addParameterGroup(&BatteryGroup);
    iotWebConf.addParameterGroup(&fullGroup);

    // -- Define how to handle updateServer calls.
    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
        [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });
  
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
    server.on("/data", HTTP_GET, []() { handleData(); });
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

      Config.SetSource(gN2KSource);

      iotWebConf.saveConfig();
      gSaveParams = false;
  }
}

void handleData() {
    String _json = "{";
    _json += "\"rssi\":\"" + String(WiFi.RSSI()) + "\",";
    _json += "\"voltage\":\"" + String(gBattery.voltage(), 2) + "\",";
    _json += "\"current\":\"" + String(gBattery.current(), 2) + "\",";
    _json += "\"avgCurrent\":\"" + String(gBattery.averageCurrent(), 2) + "\",";
    _json += "\"soc\":\"" + String(gBattery.soc() * 100, 2) + "\",";
    if (gBattery.tTg() != INFINITY) {
        _json += "\"tTg\":\"" + String(gBattery.tTg() / 3600) + "\",";
    }
    else {
		_json += "\"tTg\":\"888888\",";
	}
    _json += "\"isFull\":\"" + String(gBattery.isFull() ? "true" : "false") + "\",";
    _json += "\"temperature\":\"" + String(gBattery.temperatur()) + "\",";
    _json += "\"batteryType\":\"" + String(gBatteryType) + "\",";
    _json += "\"batteryVoltage\":\"" + String(gBatteryVoltage) + "\",";
    _json += "\"batteryChemistry\":\"" + String(gBatteryChemistry) + "\",";
    _json += "\"capacity\":\"" + String(gCapacityAh) + "\",";
    _json += "\"chargeEfficiency\":\"" + String(gChargeEfficiencyPercent) + "\",";
    _json += "\"minSoc\":\"" + String(gMinPercent) + "\",";
    _json += "\"tailCurrent\":\"" + String(gTailCurrentmA) + "\",";
    _json += "\"fullVoltage\":\"" + String(gFullVoltagemV) + "\",";
    _json += "\"fullDelay\":\"" + String(gFullDelayS) + "\",";
    _json += "\"currentThreshold\":\"" + String(gCurrentThreshold) + "\",";
    _json += "\"shuntResistance\":\"" + String(gShuntResistancemR) + "\",";
    _json += "\"maxCurrent\":\"" + String(gMaxCurrentA) + "\",";
    _json += "\"voltageCalibrationFactor\":\"" + String(gVoltageCalibrationFactor) + "\",";
    _json += "\"currentCalibrationFactor\":\"" + String(gCurrentCalibrationFactor) + "\"";
    _json += "}";
    server.send(200, "text/plain", _json);
}

class MyHtmlRootFormatProvider : public HtmlRootFormatProvider {
protected:
    virtual String getScriptInner() {
        String _s = HtmlRootFormatProvider::getScriptInner();
        _s.replace("{millisecond}", "5000");
        _s += F("function updateData(jsonData) {\n");
        _s += F("   document.getElementById('RSSIValue').innerHTML = jsonData.rssi + \"dBm\" \n");
		_s += F("   document.getElementById('VoltageValue').innerHTML = jsonData.voltage + \"V\" \n");
		_s += F("   document.getElementById('CurrentValue').innerHTML = jsonData.current + \"A\" \n");
		_s += F("   document.getElementById('AverageCurrentValue').innerHTML = jsonData.avgCurrent + \"A\" \n");
		_s += F("   document.getElementById('SocValue').innerHTML = jsonData.soc + \"%\" \n");
		_s += F("   document.getElementById('tTgValue').innerHTML = jsonData.tTg + \"h\" \n");
		_s += F("   document.getElementById('isFullValue').innerHTML = jsonData.isFull \n");
		_s += F("   document.getElementById('TemperatureValue').innerHTML = jsonData.temperature + \"&deg;C\" \n");

        _s += F("}\n");

        return _s;
    }
};

void handleRoot() {
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal()){
        return;
    }

    MyHtmlRootFormatProvider rootFormatProvider;

    String _response = "";
    _response += rootFormatProvider.getHtmlHead(iotWebConf.getThingName());
    _response += rootFormatProvider.getHtmlStyle();
    _response += rootFormatProvider.getHtmlHeadEnd();
    _response += rootFormatProvider.getHtmlScript();

    _response += rootFormatProvider.getHtmlTable();
    _response += rootFormatProvider.getHtmlTableRow() + rootFormatProvider.getHtmlTableCol();

    _response += F("<fieldset align=left style=\"border: 1px solid\">\n");
    _response += F("<table border=\"0\" align=\"center\" width=\"100%\">\n");
    _response += F("<tr><td align=\"left\"> </td></td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr>\n");
    _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlFieldsetEnd();

    _response += rootFormatProvider.getHtmlFieldset("Temperature");
    _response += rootFormatProvider.getHtmlTable();
	_response += rootFormatProvider.getHtmlTableRowSpan("Battery Voltage", "VoltageValue", "no data");
	_response += rootFormatProvider.getHtmlTableRowSpan("Shunt current", "CurrentValue", "no data");
	_response += rootFormatProvider.getHtmlTableRowSpan("Avg consumption", "AverageCurrentValue", "no data");
	_response += rootFormatProvider.getHtmlTableRowSpan("State of charge", "SocValue", "no data");
	_response += rootFormatProvider.getHtmlTableRowSpan("Time to go", "tTgValue", "no data");
	_response += rootFormatProvider.getHtmlTableRowSpan("Battery full", "isFullValue", "no data");
	_response += rootFormatProvider.getHtmlTableRowSpan("Temperature", "TemperatureValue", "no data");
	_response += rootFormatProvider.getHtmlTableEnd();
	_response += rootFormatProvider.getHtmlFieldsetEnd();

	_response += rootFormatProvider.getHtmlFieldset("Shunt configuration");
	_response += rootFormatProvider.getHtmlTable();
	_response += rootFormatProvider.getHtmlTableRowSpan("Shunt resistance", "shuntResistance", String(gShuntResistancemR, 3) + "m&#8486;");
	_response += rootFormatProvider.getHtmlTableRowSpan("Shunt max current", "maxCurrent", String(gMaxCurrentA) + "A");
	_response += rootFormatProvider.getHtmlTableEnd();
	_response += rootFormatProvider.getHtmlFieldsetEnd();

	_response += rootFormatProvider.getHtmlFieldset("Battery configuration");
	_response += rootFormatProvider.getHtmlTable();
	_response += rootFormatProvider.getHtmlTableRowSpan("Type", "BatType", String(BatTypeNames[gBatteryType]));
	_response += rootFormatProvider.getHtmlTableRowSpan("Capacity", "battCapacity", String(gCapacityAh) + "Ah");
	_response += rootFormatProvider.getHtmlTableRowSpan("Efficiency", "chargeEfficiency", String(gChargeEfficiencyPercent) + "%");
	_response += rootFormatProvider.getHtmlTableRowSpan("Min SOC", "minSoc", String(gMinPercent) + "%");
	_response += rootFormatProvider.getHtmlTableRowSpan("Tail current", "tailCurrent", String((gTailCurrentmA / 1000), 3) + "A");
	_response += rootFormatProvider.getHtmlTableRowSpan("Full voltage", "fullVoltage", String((gFullVoltagemV / 1000), 2) + "V");
	_response += rootFormatProvider.getHtmlTableRowSpan("Full delay", "fullDelay", String(gFullDelayS) + "s");
	_response += rootFormatProvider.getHtmlTableEnd();
	_response += rootFormatProvider.getHtmlFieldsetEnd();

	_response += rootFormatProvider.getHtmlFieldset("Network");
	_response += rootFormatProvider.getHtmlTable();
	_response += rootFormatProvider.getHtmlTableRowText("MAC Address", WiFi.macAddress());
	_response += rootFormatProvider.getHtmlTableRowText("IP Address", WiFi.localIP().toString().c_str());
	_response += rootFormatProvider.getHtmlTableEnd();
	_response += rootFormatProvider.getHtmlFieldsetEnd();

	_response += rootFormatProvider.addNewLine(2);

	_response += rootFormatProvider.getHtmlTable();
	_response += rootFormatProvider.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration.");
    _response += rootFormatProvider.getHtmlTableRowText(rootFormatProvider.getHtmlVersion(Version));
    _response += rootFormatProvider.getHtmlTableEnd();

    _response += rootFormatProvider.getHtmlTableColEnd() + rootFormatProvider.getHtmlTableRowEnd();
    _response += rootFormatProvider.getHtmlTableEnd();
    _response += rootFormatProvider.getHtmlEnd();

	server.send(200, "text/html", _response);
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

    gN2KSource = Config.Source();
    gN2KSID = Config.SID();
    gN2KInstance = Config.Instance();
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
