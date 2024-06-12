

// #define DEBUG_WIFI(m) SERIAL_DBG.print(m)

#include <Arduino.h>
#include <ArduinoOTA.h>
#if ESP32
#include <WiFi.h>
// #include <esp_wifi.h>
 #else
#include <ESP8266WiFi.h>   

#endif

#include "common.h"
#include "webHandling.h"
#include "statusHandling.h"
#include "favicon.h"

#include <DNSServer.h>
#include <IotWebConf.h>
#include <IotWebConfAsyncClass.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRoot.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

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
void handleSetRuntime(AsyncWebServerRequest* request);
void onSetSoc(AsyncWebServerRequest* request);
void onProgress(size_t prg, size_t sz);
void handleData(AsyncWebServerRequest* request);
void handleRoot(AsyncWebServerRequest* request);
void handleStatistics(AsyncWebServerRequest* request);
void convertParams();
bool connectAp(const char* apName, const char* password);
void connectWifi(const char* ssid, const char* password);

// -- Callback methods.
void configSaved();

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);
AsyncUpdateServer AsyncUpdater;

bool gParamsChanged = true;
bool gSaveParams = false;
bool startAPMode = true;

uint16_t gCapacityAh;
uint16_t gChargeEfficiencyPercent;
uint16_t gMinPercent;
uint16_t gTailCurrentmA;
uint16_t gFullVoltagemV;
uint16_t gFullDelayS;
float gCurrentThreshold;
float gShuntResistanceR;
uint16_t gMaxCurrentA;
char gCustomName[64] = "NMEA-Batterymonitor";
tN2kBatType gBatteryType = N2kDCbt_AGM;
tN2kBatNomVolt gBatteryVoltage = N2kDCbnv_12v;
tN2kBatChem gBatteryChemistry = N2kDCbc_LeadAcid;


// -- We can add a legend to the separator
IotWebConf iotWebConf(gCustomName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);
NMEAConfig Config = NMEAConfig();

char APModeValue[STRING_LEN];
iotwebconf::CheckboxParameter APModeParam = iotwebconf::CheckboxParameter("start AP only at boot sequence", "APModeID", APModeValue, STRING_LEN, false);

char maxCurrentValue[NUMBER_LEN];
char VoltageCalibrationFactorValue[NUMBER_LEN];
char CurrentCalibrationFactorValue[NUMBER_LEN];
char shuntResistanceValue[NUMBER_LEN];

iotwebconf::ParameterGroup ShuntGroup = iotwebconf::ParameterGroup("ShuntGroup","Shunt");
iotwebconf::NumberParameter shuntResistance = iotwebconf::NumberParameter("Shunt resistance [&#8486;]", "shuntR", shuntResistanceValue, NUMBER_LEN, "0.750", "0..100", "min='0.001' max='100' step='0.001'");
iotwebconf::NumberParameter maxCurrent = iotwebconf::NumberParameter("Expected max current [A]", "maxA", maxCurrentValue, NUMBER_LEN, "100", "1..500", "min='1' max='500' step='1'");
iotwebconf::NumberParameter VoltageCalibrationFactor = iotwebconf::NumberParameter("Voltage calibration factor", "VoltageCalibrationFactor", VoltageCalibrationFactorValue, NUMBER_LEN, "1.0000", "-5.00000 - 5.00000", "min='-5.00000' max='5.00000' step='0.00001'");
iotwebconf::NumberParameter CurrentCalibrationFactor = iotwebconf::NumberParameter("Current calibration factor", "CurrentCalibrationFactor", CurrentCalibrationFactorValue, NUMBER_LEN, "1.0000", "-5.00000 - 5.00000", "min='-5.00000' max='5.00000' step='0.00001'");

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
        _s += F("</br><form action='/reboot' method='get'><button type='submit'>Reboot</button></form>");
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

    iotWebConf.addSystemParameter(&APModeParam);

    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { AsyncUpdater.setup(&server, updatePath, onProgress); },
        [](const char* userName, char* password) { AsyncUpdater.updateCredentials(userName, password); });
  
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;

    iotWebConf.setApConnectionHandler(&connectAp);
    iotWebConf.setWifiConnectionHandler(&connectWifi);

    // -- Initializing the configuration.
    iotWebConf.init();
  
    convertParams();

    // -- Set up required URL handlers on the web server.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { handleRoot(request); });
    server.on("/stats", HTTP_GET, [](AsyncWebServerRequest* request) { handleStatistics(request); });
    server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
            AsyncWebRequestWrapper asyncWebRequestWrapper(request);
            iotWebConf.handleConfig(&asyncWebRequestWrapper);
        }
    );
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
            AsyncWebServerResponse* response = request->beginResponse_P(200, "image/x-icon", favicon_ico, sizeof(favicon_ico));
            request->send(response);
        }
    );
    server.on("/setruntime", HTTP_GET, [](AsyncWebServerRequest* request) { 
        handleSetRuntime(request); 
        }
    );
    server.on("/setsoc", HTTP_POST, [](AsyncWebServerRequest* request) { 
        onSetSoc(request); 
        }
    );
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) { handleData(request); });
	server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* request) {
            AsyncWebServerResponse* response = request->beginResponse(302, "text/plain", "please wait while the device is rebooting ...");
            response->addHeader("Refresh", "15");
            response->addHeader("Location", "/");
            request->client()->setNoDelay(true); // Disable Nagle's algorithm so the client gets the 302 response immediately
            request->send(response);
		    delay(500);
		    ESP.restart();
		}
	);

    server.onNotFound([](AsyncWebServerRequest* request) {
            AsyncWebRequestWrapper asyncWebRequestWrapper(request);
            iotWebConf.handleNotFound(&asyncWebRequestWrapper);
        }
    );

    WebSerial.begin(&server, "/webserial");
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

  if ((iotWebConf.getState() == iotwebconf::OnLine) && (APModeParam.isChecked()) && startAPMode) {
	  Serial.println(F("start AP mode only at boot sequence"));
	  startAPMode = false;
  }


}

void onProgress(size_t prg, size_t sz) {
    static size_t lastPrinted = 0;
    size_t currentPercent = (prg * 100) / sz;

    if (currentPercent % 5 == 0 && currentPercent != lastPrinted) {
        Serial.printf("Progress: %d%%\n", currentPercent);
        WebSerial.printf("Progress: %d%%\n", currentPercent);
        lastPrinted = currentPercent;
    }
}

void onSetSoc(AsyncWebServerRequest* request) {
    String _soc = request->arg("soc");
    _soc.trim();
    if (!_soc.isEmpty()) {
        float _socVal = _soc.toInt() / 100.00;
        gBattery.setBatterySoc(_socVal);
    }

    request->send(200, "text/html", SOC_RESPONSE);
}

void handleSetRuntime(AsyncWebServerRequest* request) {

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

    request->send(200, "text/html", page);
}

void handleData(AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    response->addHeader("Server", "ESP Async Web Server");
    JsonVariant& json_ = response->getRoot();

	json_["rssi"] = WiFi.RSSI();
	json_["voltage"] = String(gBattery.voltage(), 2);
	json_["current"] = String(gBattery.current(), 2);
	json_["avgCurrent"] = String(gBattery.averageCurrent(), 2);
	json_["soc"] = String(gBattery.soc() * 100, 1);
    if (gBattery.tTg() != 4294967295) {
        std::string _hours = std::to_string(gBattery.tTg() / 3600);
        std::string _minutes = std::to_string((gBattery.tTg() % 3600) / 60);
        _minutes.insert(0, 2 - _minutes.length(), '0');
        _hours.insert(0, 2 - _hours.length(), '0');
		json_["tTg"] = _hours + ":" + _minutes.c_str();
	}
    else {
		json_["tTg"] = "00:00";
    }
	json_["isFull"] = gBattery.isFull();
	json_["temperature"] = String(gBattery.temperatur(), 2);
	json_["batteryType"] = gBatteryType;
	json_["batteryVoltage"] = gBatteryVoltage;
	json_["batteryChemistry"] = gBatteryChemistry;
	json_["capacity"] = gCapacityAh;
	json_["chargeEfficiency"] = gChargeEfficiencyPercent;
	json_["minSoc"] = gMinPercent;
	json_["tailCurrent"] = String((gTailCurrentmA / 1000.00f), 3);
    json_["fullVoltage"] = String((gFullVoltagemV / 1000.00f), 2);
	json_["fullDelay"] = gFullDelayS;
	json_["currentThreshold"] = String(gCurrentThreshold, 3);
	json_["shuntResistance"] = gShuntResistanceR;
	json_["maxCurrent"] = gMaxCurrentA;
	json_["voltageCalibrationFactor"] = gVoltageCalibrationFactor;
	json_["currentCalibrationFactor"] = gCurrentCalibrationFactor;

    Statistics stats_ = gBattery.statistics();

    json_["consumedAh"] = String(stats_.consumedAs / 3600.00f, 3); // Ah
    json_["consumedAs"] = String(stats_.consumedAs, 2); //As
    json_["deepestDischarge"] = String(stats_.deepestDischarge / 1000.00f, 3); // Ah
    json_["lastDischarge"] = String(stats_.lastDischarge / 1000.00f, 3); // Ah
    json_["averageDischarge"] = String(stats_.averageDischarge / 1000.00f, 3); //Ah
    json_["numChargeCycles"] = stats_.numChargeCycles;
    json_["numFullDischarge"] = stats_.numFullDischarge;
    json_["sumApHDrawn"] = stats_.sumApHDrawn;  //Ah
    json_["minBatVoltage"] = String(stats_.minBatVoltage / 1000.00f, 2); // V
    json_["maxBatVoltage"] = String(stats_.maxBatVoltage / 1000.00f, 2); // V

    std::string _hours2 = std::to_string(gBattery.tTg() / 3600);
    std::string _minutes2 = std::to_string((gBattery.tTg() % 3600) / 60);
    _minutes2.insert(0, 2 - _minutes2.length(), '0');
    _hours2.insert(0, 2 - _hours2.length(), '0');
    json_["TimeSinceLastFull"] = _hours2 + ":" + _minutes2.c_str();
    json_["secsSinceLastFull"] = stats_.secsSinceLastFull;

    json_["numAutoSyncs"] = stats_.numAutoSyncs;
    json_["numLowVoltageAlarms"] = stats_.numLowVoltageAlarms;
    json_["numHighVoltageAlarms"] = stats_.numHighVoltageAlarms;
    json_["amountDischargedEnergy"] = String(stats_.amountDischargedEnergy, 3); //kWh
    json_["amountChargedEnergy"] = String(stats_.amountChargedEnergy, 3); // kWh
    json_["deepestTemperatur"] = String(stats_.deepestTemperatur, 2); //�C
    json_["highestTemperatur"] = String(stats_.highestTemperatur, 2); //�C

    response->setLength();
	request->send(response);
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

void handleRoot(AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)) {
        return;
    }

    std::string content_;
    MyHtmlRootFormatProvider fp_;

	content_ += fp_.getHtmlHead(iotWebConf.getThingName()).c_str();
	content_ += fp_.getHtmlStyle().c_str();
	content_ += fp_.getHtmlHeadEnd().c_str();
	content_ += fp_.getHtmlScript().c_str();
	content_ += fp_.getHtmlTable().c_str();
    content_ += fp_.getHtmlTableRow().c_str();
    content_ += fp_.getHtmlTableCol().c_str();

	content_ += String(F("<fieldset align=left style=\"border: 1px solid\">\n")).c_str();
	content_ += String(F("<table border=\"0\" align=\"center\" width=\"100%\">\n")).c_str();
	content_ += String(F("<tr><td align=\"left\"> </td></td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr>\n")).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();

	content_ += fp_.getHtmlFieldset("Running values").c_str();
	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRowSpan("Voltage: ", "no data", "VoltageValue").c_str();
	content_ += fp_.getHtmlTableRowSpan("Current: ", "no data", "CurrentValue").c_str();
	content_ += fp_.getHtmlTableRowSpan("Avg current: ", "no data", "AverageCurrentValue").c_str();
	content_ += fp_.getHtmlTableRowSpan("State of charge: ", "no data", "SocValue").c_str();
	content_ += fp_.getHtmlTableRowSpan("Time to go: ", "no data", "tTgValue").c_str();
	content_ += fp_.getHtmlTableRowSpan("Battery full: ", "no data", "isFullValue").c_str();
	content_ += fp_.getHtmlTableRowSpan("Temperature: ", "no data", "TemperatureValue").c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();

	content_ += fp_.getHtmlFieldset("Shunt configuration").c_str();
	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRowSpan("Shunt resistance:", String(gShuntResistanceR, 3) + "&#8486;", "shuntResistance").c_str();
	content_ += fp_.getHtmlTableRowSpan("Shunt max current:", String(gMaxCurrentA) + "A", "maxCurrent").c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();

	content_ += fp_.getHtmlFieldset("Battery configuration").c_str();
	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRowSpan("Type:", String(BatTypeNames[gBatteryType]), "BatType").c_str();
	content_ += fp_.getHtmlTableRowSpan("Capacity:", String(gCapacityAh) + "Ah", "battCapacity").c_str();
	content_ += fp_.getHtmlTableRowSpan("Efficiency:", String(gChargeEfficiencyPercent) + "%", "chargeEfficiency").c_str();
	content_ += fp_.getHtmlTableRowSpan("Min SOC:", String(gMinPercent) + "%", "minSoc").c_str();
    float mA_ = gTailCurrentmA;
    float mV_ = gFullVoltagemV;
    content_ += fp_.getHtmlTableRowSpan("Tail current:", String((mA_ / 1000), 3) + "A", "tailCurrent").c_str();
	content_ += fp_.getHtmlTableRowSpan("Full voltage:", String((mV_ / 1000), 2) + "V", "fullVoltage").c_str();
	content_ += fp_.getHtmlTableRowSpan("Full delay:", String(gFullDelayS) + "s", "fullDelay").c_str();
	content_ += fp_.getHtmlTableRowSpan("Manufacturer:", String(BatteryManufacturerValue), "BattManufacturer").c_str();
	content_ += fp_.getHtmlTableRowSpan("Replacment date:", String(BatteryReplacmentDateValue), "BattDate").c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();

	content_ += fp_.getHtmlFieldset("Network").c_str();
	content_ += fp_.getHtmlTable().c_str();
	content_ += fp_.getHtmlTableRowText("MAC Address:", WiFi.macAddress()).c_str();
	content_ += fp_.getHtmlTableRowText("IP Address:", WiFi.localIP().toString().c_str()).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlFieldsetEnd().c_str();

	content_ += fp_.addNewLine(2).c_str();
    
	content_ += fp_.getHtmlTable().c_str();
    content_ += fp_.getHtmlTableRowText("Go to <a href = 'setruntime'>runtime modification page</a> to change runtime data.").c_str();
	content_ += fp_.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration.").c_str();
    content_ += fp_.getHtmlTableRowText("Go to <a href = 'webserial'>sensor monitoring</a> page.").c_str();
	
    content_ += fp_.getHtmlTableRowText(fp_.getHtmlVersion(Version)).c_str();
	content_ += fp_.getHtmlTableEnd().c_str();

    content_ += fp_.getHtmlTableColEnd().c_str();
    content_ += fp_.getHtmlTableRowEnd().c_str();
	content_ += fp_.getHtmlTableEnd().c_str();
	content_ += fp_.getHtmlEnd().c_str();

    AsyncWebServerResponse* response = request->beginChunkedResponse("text/html", [content_](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {

        std::string chunk_ = "";
        size_t len_ = min(content_.length() - index, maxLen);
        if (len_ > 0) {
            chunk_ = content_.substr(index, len_);
            chunk_.copy((char*)buffer, chunk_.length());
        }
        if (index + len_ <= content_.length())
            return chunk_.length();
        else
            return 0;

        });
    response->setContentLength(content_.length());
    response->addHeader("Server", "ESP Async Web Server");
    request->send(response);
}


class MyHtmlRootFormatProviderStatistics : public HtmlRootFormatProvider {
protected:
    virtual String getScriptInner() {
        String _s = HtmlRootFormatProvider::getScriptInner();
        _s.replace("{millisecond}", "5000");
        _s += F("function updateData(jsonData) {\n");
        _s += F("   document.getElementById('minBatVoltage').innerHTML = jsonData.minBatVoltage + \"V\" \n");
        _s += F("   document.getElementById('maxBatVoltage').innerHTML = jsonData.maxBatVoltage + \"V\" \n");

        _s += F("   document.getElementById('amountDischargedEnergy').innerHTML = jsonData.amountDischargedEnergy + \"kWh\" \n");
        _s += F("   document.getElementById('amountChargedEnergy').innerHTML = jsonData.amountChargedEnergy + \"kWh\" \n");

        _s += F("   document.getElementById('consumedAh').innerHTML = jsonData.consumedAh + \"kWh\" \n");
        _s += F("   document.getElementById('averageDischarge').innerHTML = jsonData.averageDischarge + \"Ah\" \n");
        _s += F("   document.getElementById('deepestDischarge').innerHTML = jsonData.deepestDischarge + \"Ah\" \n");
        _s += F("   document.getElementById('lastDischarge').innerHTML = jsonData.lastDischarge + \"Ah\" \n");

        _s += F("   document.getElementById('deepestTemperatur').innerHTML = jsonData.deepestTemperatur + \"&deg;C\" \n");
        _s += F("   document.getElementById('highestTemperatur').innerHTML = jsonData.highestTemperatur + \"&deg;C\" \n");
        _s += F("   document.getElementById('TimeSinceLastFull').innerHTML = jsonData.TimeSinceLastFull + \"h\" \n");

        _s += F("   document.getElementById('numChargeCycles').innerHTML = jsonData.numChargeCycles \n");
        _s += F("   document.getElementById('numAutoSyncs').innerHTML = jsonData.numAutoSyncs \n");

        _s += F("}\n");

        return _s;
    }
};

void handleStatistics(AsyncWebServerRequest* request) {
    std::string content_;
    MyHtmlRootFormatProviderStatistics fp_;

    content_ += fp_.getHtmlHead(iotWebConf.getThingName()).c_str();
    content_ += fp_.getHtmlStyle().c_str();
    content_ += fp_.getHtmlHeadEnd().c_str();
    content_ += fp_.getHtmlScript().c_str();
    content_ += fp_.getHtmlTable().c_str();
    content_ += fp_.getHtmlTableRow().c_str();
    content_ += fp_.getHtmlTableCol().c_str();

    content_ += fp_.getHtmlFieldset("Statistics").c_str();
    content_ += fp_.getHtmlTable().c_str();

    content_ += fp_.getHtmlTableRowSpan("min Volt: ", "no data", "minBatVoltage").c_str();
    content_ += fp_.getHtmlTableRowSpan("max Volt: ", "no data", "maxBatVoltage").c_str();

    content_ += fp_.getHtmlTableRowSpan("Consumed Ah: ", "no data", "consumedAh").c_str();
    content_ += fp_.getHtmlTableRowSpan("Charged energy:", "no data", "amountChargedEnergy").c_str();
    content_ += fp_.getHtmlTableRowSpan("Discharged energy:", "no data", "amountDischargedEnergy").c_str();
    content_ += fp_.getHtmlTableRowSpan("Avg. discharged energy:", "no data", "averageDischarge").c_str();
    content_ += fp_.getHtmlTableRowSpan("Last discharged energy:", "no data", "lastDischarge").c_str();
    content_ += fp_.getHtmlTableRowSpan("Deepest discharged energy:", "no data", "deepestDischarge").c_str();

    content_ += fp_.getHtmlTableRowSpan("min Temperatur: ", "no data", "deepestTemperatur").c_str();
    content_ += fp_.getHtmlTableRowSpan("max Temperatur: ", "no data", "highestTemperatur").c_str();

    content_ += fp_.getHtmlTableRowSpan("Charge cycles: ", "no data", "numChargeCycles").c_str();
    content_ += fp_.getHtmlTableRowSpan("Auto syncs: ", "no data", "numAutoSyncs").c_str();
    content_ += fp_.getHtmlTableRowSpan("Time since last full:", "no data", "TimeSinceLastFull").c_str();

    content_ += fp_.getHtmlTableEnd().c_str();
    content_ += fp_.getHtmlFieldsetEnd().c_str();

    content_ += fp_.addNewLine(2).c_str();

    content_ += fp_.getHtmlTable().c_str();
    content_ += fp_.getHtmlTableRowText("Go to <a href = 'setruntime'>runtime modification page</a> to change runtime data.").c_str();
    content_ += fp_.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration.").c_str();
    content_ += fp_.getHtmlTableRowText("Go to <a href = 'webserial'>sensor monitoring</a> page.").c_str();

    content_ += fp_.getHtmlTableRowText(fp_.getHtmlVersion(Version)).c_str();
    content_ += fp_.getHtmlTableEnd().c_str();

    content_ += fp_.getHtmlTableColEnd().c_str();
    content_ += fp_.getHtmlTableRowEnd().c_str();
    content_ += fp_.getHtmlTableEnd().c_str();
    content_ += fp_.getHtmlEnd().c_str();

    AsyncWebServerResponse* response = request->beginChunkedResponse("text/html", [content_](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {

        std::string chunk_ = "";
        size_t len_ = min(content_.length() - index, maxLen);
        if (len_ > 0) {
            chunk_ = content_.substr(index, len_);
            chunk_.copy((char*)buffer, chunk_.length());
        }
        if (index + len_ <= content_.length())
            return chunk_.length();
        else
            return 0;

        });
    response->setContentLength(content_.length());
    response->addHeader("Server", "ESP Async Web Server");
    request->send(response);
}

void convertParams() {
    gShuntResistanceR = atof(shuntResistanceValue);
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
  

  if (!APModeParam.isChecked()) {
	  Serial.println(F("start allways AP mode"));
	  startAPMode = true;
  }
  Serial.println(F("Parameters are saved"));
} 

bool connectAp(const char* apName, const char* password){
    if (startAPMode){
        Serial.println("starting AP mode ....");
        return WiFi.softAP(apName, password);    
    }
	else {
        Serial.println(F("start AP mode only at boot sequence"));
        WiFi.mode(WIFI_MODE_STA);
		return true;
	}
}

void connectWifi(const char* ssid, const char* password){
	Serial.println("Connecting to WiFi ...");
    WiFi.begin(ssid, password);
}

