// #define DEBUG_WIFI(m) SERIAL_DBG.print(m)

#include <Arduino.h>
#include <ArduinoOTA.h>

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error "Unsupported platform"
#endif

#include "common.h"
#include "webHandling.h"
#include "statusHandling.h"
#include "favicon.h"
#include "neotimer.h"
#include "BatteryConfig.h"
#include "NMEAConfig.h"
#include "html.h"

#include <DNSServer.h>
#include <IotWebConfAsyncTab.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRootTab.h>
#include <RebootManager.h>

#include <N2kTypes.h>

// -- Method declarations.
void handleSetRuntime(AsyncWebServerRequest* request);
void onSetSoc(AsyncWebServerRequest* request);
void onResetStatistics(AsyncWebServerRequest* request);
void onProgress(size_t prg, size_t sz);
void handleData(AsyncWebServerRequest* request);
void handleRoot(AsyncWebServerRequest* request);
void handlePost(AsyncWebServerRequest* request);
void handleReboot(AsyncWebServerRequest* request);
void convertParams();
void connectWifi(const char* ssid, const char* password);
iotwebconf::WifiAuthInfo* handleConnectWifiFailure();

// -- Callback methods.
void configSaved();

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebServerWrapper asyncWebServerWrapper(&server);
AsyncUpdateServer AsyncUpdater;
Neotimer APModeTimer = Neotimer();

bool ParamsChanged = true;
bool SaveParams = false;
bool startAPMode = true;
uint8_t APModeOfflineTime = 0;
bool ShouldReboot = false;

char CustomName[64] = "NMEA-Batterymonitor";

AsyncIotWebConfTab iotWebConf(CustomName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);
NMEAConfig nmeaConfig  = NMEAConfig();
BatteryConfig batteryConfig = BatteryConfig();
BatteryFullConfig batteryFullConfig = BatteryFullConfig();
ShuntConfig shuntConfig = ShuntConfig();

char APModeValue[STRING_LEN];
iotwebconf::CheckboxParameter APModeParam = iotwebconf::CheckboxParameter("start AP only at boot sequence", "APModeID", APModeValue, STRING_LEN, false);

char APModeOfflineValue[STRING_LEN];
iotwebconf::NumberParameter APModeOfflineParam = iotwebconf::NumberParameter("AP offline mode after (minutes)", "APModeOffline", APModeOfflineValue, NUMBER_LEN, "0", "0..30", "min='0' max='30', step='1'");

class CustomHtmlFormatProvider : public AsyncTabHtmlFormatProvider {
public:
    CustomHtmlFormatProvider(std::vector<AsyncTabInfo>* tabs, int minWidth = 500, int maxWidth = 600)
        : AsyncTabHtmlFormatProvider(tabs, minWidth, maxWidth) {
    }

protected:
    String getFormEnd() override {
        String s_ = AsyncTabHtmlFormatProvider::getFormEnd();

        // Add form end buttons from PROGMEM
        char buffer_[1024];
        strcpy_P(buffer_, html_form_end);
        s_ += buffer_;

        return s_;
    }

    String getScriptInner() override {
        String s_ = AsyncTabHtmlFormatProvider::getScriptInner();
        s_.replace("{millisecond}", "2000");

        // Add updateData function from PROGMEM
        char buffer_[2048];
        strcpy_P(buffer_, html_update_data_function);
        s_ += buffer_;

        return s_;
    }
};
CustomHtmlFormatProvider* customHtmlFormatProvider = nullptr;

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

     
    iotWebConf.setStatusPin(STATUS_PIN,ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);

    // === TABS KONFIGURIEREN ===
    // System Tab soll an letzter Stelle sein
    iotWebConf.setSystemTabPosition(-1);  // -1 = letzte Position
    iotWebConf.setSystemTabName("System");

    // Battery Tab
    iotWebConf.addParameterGroup(&batteryConfig, "Battery");
    iotWebConf.addParameterGroup(&batteryFullConfig, "Battery");

    // Shunt Tab
    iotWebConf.addParameterGroup(&shuntConfig, "Shunt");

    // NMEA Tab
    iotWebConf.addParameterGroup(&nmeaConfig, "NMEA");

    // System Parameters
    iotWebConf.addSystemParameter(&APModeParam);
    iotWebConf.addSystemParameter(&APModeOfflineParam);

    // CustomHtmlFormatProvider mit Tabs-Vektor erstellen
    auto customProvider = new CustomHtmlFormatProvider(iotWebConf.getTabsVector());
    customProvider->setContainerWidth(700, 700);
    iotWebConf.setHtmlFormatProvider(customProvider);

    iotWebConf.setupUpdateServer(
        [](const char* updatePath) { AsyncUpdater.setup(&server, updatePath, onProgress); },
        [](const char* userName, char* password) { AsyncUpdater.updateCredentials(userName, password); });
  
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;

    iotWebConf.setWifiConnectionHandler(&connectWifi);
    iotWebConf.setWifiConnectionFailedHandler(&handleConnectWifiFailure);

    // -- Initializing the configuration.
    iotWebConf.init();
  
    convertParams();

    // -- Set up required URL handlers on the web server.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { handleRoot(request); });
    
    server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
        auto* asyncWebRequestWrapper = new AsyncWebRequestWrapper(request);
        iotWebConf.handleConfig(asyncWebRequestWrapper);
        }
    );

    IconHandler::registerHandlers(&server, favicon_ico, sizeof(favicon_ico));

    server.on("/setsoc", HTTP_POST, [](AsyncWebServerRequest* request) { 
            onSetSoc(request); 
        }
    );
    server.on("/resetstats", HTTP_POST, [](AsyncWebServerRequest* request) {
            onResetStatistics(request);
        }
    );
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) { handleData(request); });

    server.on("/post", HTTP_ANY, [](AsyncWebServerRequest* request) { handlePost(request); });
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* request) { handleReboot(request); });



    server.onNotFound([](AsyncWebServerRequest* request) {
            AsyncWebRequestWrapper asyncWebRequestWrapper(request);
            iotWebConf.handleNotFound(&asyncWebRequestWrapper);
        }
    );

    WebSerial.begin(&server, "/webserial");

    if (APModeOfflineTime > 0) {
        APModeTimer.start(APModeOfflineTime * 60 * 1000);
    }

    Serial.println("Ready.");
}

void wifiLoop() {
      // -- doLoop should be called as frequently as possible.
      iotWebConf.doLoop();
      ArduinoOTA.handle();

    if (SaveParams) {
        Serial.println(F("Parameters are changed,save them"));

        iotWebConf.saveConfig();
        SaveParams = false;
    }

    if ((iotWebConf.getState() == iotwebconf::OnLine) && (APModeParam.isChecked()) && startAPMode) {
	    Serial.println(F("start AP mode only at boot sequence"));
	    startAPMode = false;
    }

    if (APModeTimer.done()) {
		Serial.println(F("AP mode offline time reached"));
		iotWebConf.goOffLine();
		APModeTimer.stop();
	}

	if (ShouldReboot || AsyncUpdater.isFinished()) {
		Serial.println(F("Rebooting..."));
        batteryStatus.writeStats();
		delay(1000);
		ESP.restart();
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
    if (!request->hasArg("soc")) {
        request->send(400, "text/plain", "Missing SOC parameter");
        return;
    }

    String socStr_ = request->arg("soc");
    socStr_.trim();

    if (socStr_.isEmpty()) {
        request->send(400, "text/plain", "SOC value is empty");
        return;
    }

    int socInt_ = socStr_.toInt();
    if (socInt_ < 0 || socInt_ > 100) {
        request->send(400, "text/plain", "SOC must be between 0 and 100");
        return;
    }

    float socVal_ = socInt_ / 100.0f;
    batteryStatus.setBatterySoc(socVal_);
    batteryStatus.writeStats();

    Serial.printf("SOC set to %d%%\n", socInt_);
    WebSerial.printf("SOC set to %d%%\n", socInt_);

    // Use PROGMEM string from html.h
    request->send_P(200, "text/html", html_soc_response);
}

void onResetStatistics(AsyncWebServerRequest* request) {
    batteryStatus.resetStats();

    Serial.println("Statistics reset");
    WebSerial.println("Statistics reset");

    // Use PROGMEM string from html.h
    request->send_P(200, "text/html", html_stats_reset_response);
}

void handleData(AsyncWebServerRequest* request) {
	String json_ = "{";
	json_ += "\"rssi\":" + String(WiFi.RSSI()) + ",";
	json_ += "\"voltage\":" + String(batteryStatus.voltage(), 2) + ",";
	json_ += "\"current\":" + String(batteryStatus.current(), 2) + ",";
	json_ += "\"avgCurrent\":" + String(batteryStatus.averageCurrent(), 2) + ",";
	json_ += "\"soc\":" + String(batteryStatus.soc() * 100, 1) + ",";
	if (batteryStatus.tTg() != 4294967295) {
        String hours_ = String(batteryStatus.tTg() / 3600);
        String minutes_ = String((batteryStatus.tTg() % 3600) / 60);
        if (minutes_.length() < 2) minutes_ = "0" + minutes_;
        if (hours_.length() < 2) hours_ = "0" + hours_;
        json_ += "\"tTg\":\"" + hours_ + ":" + minutes_ + "\",";
	}
	else {
		json_ += "\"tTg\":\"00:00\",";
	}
    json_ += "\"mode\":\"" + String(batteryStatus.current() >= 0 ? "charging" : "discharging") + "\",";
    json_ += "\"isFull\":\"" + String(batteryStatus.isFull() ? "true" : "false") + "\",";
	json_ += "\"temperature\":" + String(batteryStatus.temperatur(), 2) + ",";
	json_ += "\"batteryType\":\"" + String(BatTypeNames[batteryConfig.Type()]) + "\",";
	json_ += "\"batteryVoltage\":\"" + String(BatNomVoltNames[batteryConfig.NomVolt()]) + "\",";
	json_ += "\"batteryChemistry\":\"" + String(BatChemNames[batteryConfig.Chemistry()]) + "\",";
	json_ += "\"capacity\":" + String(batteryConfig.Capacity_Ah()) + ",";
	json_ += "\"chargeEfficiency\":" + String(batteryConfig.ChargeEfficiency_Percent()) + ",";
	json_ += "\"minSoc\":" + String(batteryConfig.MinSoc_Percent()) + ",";
	json_ += "\"tailCurrent\":" + String((batteryFullConfig.TailCurrent_mA() / 1000.00f), 3) + ","; 
	json_ += "\"fullVoltage\":" + String((batteryFullConfig.FullVoltage_mV() / 1000.00f), 2) + ",";
	json_ += "\"fullDelay\":" + String(batteryFullConfig.FullDelay_s()) + ",";
	json_ += "\"currentThreshold\":" + String(batteryFullConfig.CurrentThreshold_A(), 3) + ",";
	json_ += "\"shuntResistance\":" + String(shuntConfig.ShuntResistance(), 5) + ",";
	json_ += "\"maxCurrent\":" + String(shuntConfig.MaxCurrent()) + ",";
	json_ += "\"voltageCalibrationFactor\":" + String(shuntConfig.VoltageCalibrationFactor()) + ","; 
	json_ += "\"currentCalibrationFactor\":" + String(shuntConfig.CurrentCalibrationFactor()) + ",";

	json_ += "\"consumedAh\":" + String(batteryStatus.statistics().consumedAs / 3600.00f, 3) + ","; // Ah
	json_ += "\"consumedAs\":" + String(batteryStatus.statistics().consumedAs, 2) + ","; //As
	json_ += "\"deepestDischarge\":" + String(batteryStatus.statistics().deepestDischarge / 1000.00f, 3) + ","; // Ah
	json_ += "\"lastDischarge\":" + String(batteryStatus.statistics().lastDischarge / 1000.00f, 3) + ","; // Ah
	json_ += "\"averageDischarge\":" + String(batteryStatus.statistics().averageDischarge / 1000.00f, 3) + ","; //Ah
	json_ += "\"numChargeCycles\":" + String(batteryStatus.statistics().numChargeCycles) + ",";
	json_ += "\"numFullDischarge\":" + String(batteryStatus.statistics().numFullDischarge) + ",";
	json_ += "\"sumApHDrawn\":" + String(batteryStatus.statistics().sumApHDrawn, 3) + ",";  //Ah
	json_ += "\"minBatVoltage\":" + String(batteryStatus.statistics().minBatVoltage / 1000.00f, 2) + ","; // V
	json_ += "\"maxBatVoltage\":" + String(batteryStatus.statistics().maxBatVoltage / 1000.00f, 2) + ","; // V

    String hours2_ = String(batteryStatus.statistics().secsSinceLastFull / 3600);
    String minutes2_ = String((batteryStatus.statistics().secsSinceLastFull % 3600) / 60);
    String seconds2_ = String(batteryStatus.statistics().secsSinceLastFull % 60);
    if (seconds2_ == "-1") {
        seconds2_ = "0";
    }
    if (minutes2_.length() < 2) minutes2_ = "0" + minutes2_;
    if (hours2_.length() < 2) hours2_ = "0" + hours2_;
    if (seconds2_.length() < 2) seconds2_ = "0" + seconds2_;
    json_ += "\"TimeSinceLastFull\":\"" + hours2_ + ":" + minutes2_ + ":" + seconds2_ + "\",";

	json_ += "\"secsSinceLastFull\":" + String(batteryStatus.statistics().secsSinceLastFull) + ",";
	json_ += "\"numAutoSyncs\":" + String(batteryStatus.statistics().numAutoSyncs) + ",";
	json_ += "\"numLowVoltageAlarms\":" + String(batteryStatus.statistics().numLowVoltageAlarms) + ",";
	json_ += "\"numHighVoltageAlarms\":" + String(batteryStatus.statistics().numHighVoltageAlarms) + ",";
	json_ += "\"amountDischargedEnergy\":" + String(batteryStatus.statistics().amountDischargedEnergy, 3) + ","; //kWh
	json_ += "\"amountChargedEnergy\":" + String(batteryStatus.statistics().amountChargedEnergy, 3) + ","; // kWh
	json_ += "\"deepestTemperatur\":" + String(batteryStatus.statistics().deepestTemperatur, 2) + ","; //�C
	json_ += "\"highestTemperatur\":" + String(batteryStatus.statistics().highestTemperatur, 2) + ""; //�C

	json_ += "}";
	request->send(200, "application/json", json_);
}

class MyHtmlRootFormatProvider : public TabHtmlRootFormatProvider {
protected:
    String getScriptInner() override {
        String s_ = TabHtmlRootFormatProvider::getScriptInner();
        s_.replace("{millisecond}", "2000");

        // Add updateData function from PROGMEM
        char buffer_[2048];
        strcpy_P(buffer_, html_update_data_function);
        s_ += buffer_;

        return s_;
    }
};

void handleRoot(AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)) {
        return;
    }

    AsyncResponseStream* response_ = request->beginResponseStream("text/html");
    MyHtmlRootFormatProvider fp_;

    // === 5 Tabs erstellen ===
    fp_.addTab("Current", "current");
    fp_.addTab("Statistics", "statistics");
    fp_.addTab("Shunt", "shunt");
    fp_.addTab("Battery", "battery");
    fp_.addTab("System", "system");

    response_->print(fp_.getHtmlHead(iotWebConf.getThingName()));
    response_->print(fp_.getIconLinks());
    response_->print(fp_.getHtmlStyle());
    response_->print(fp_.getHtmlHeadEnd());
    response_->print(fp_.getHtmlScript());
    response_->print(fp_.getHtmlTabButtons());

    // === Current Tab
    response_->print(fp_.getHtmlTabContentStart("current"));
    response_->print(F("<fieldset align=left style=\"border: 1px solid\"><table border=\"0\" align=\"center\" width=\"100%\">"));
    response_->print(F("<tr><td align=\"left\">RSSI:</td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr></table></fieldset>"));

    response_->print(fp_.getHtmlFieldset("Running values"));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowSpan("Voltage: ", "no data", "VoltageValue"));
    response_->print(fp_.getHtmlTableRowSpan("Current: ", "no data", "CurrentValue"));
    response_->print(fp_.getHtmlTableRowSpan("Avg current: ", "no data", "AverageCurrentValue"));
    response_->print(fp_.getHtmlTableRowSpan("State of charge: ", "no data", "SocValue"));
    response_->print(fp_.getHtmlTableRowSpan("Time to go: ", "no data", "tTgValue"));
    response_->print(fp_.getHtmlTableRowSpan("Battery full: ", "no data", "isFullValue"));
    response_->print(fp_.getHtmlTableRowSpan("Temperature: ", "no data", "TemperatureValue"));
    response_->print(fp_.getHtmlTableRowSpan("Mode: ", "no data", "Mode"));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.getHtmlTabContentEnd());

    // === Statistics Tab
    response_->print(fp_.getHtmlTabContentStart("statistics"));
    response_->print(fp_.getHtmlFieldset("Statistics"));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowSpan("Min voltage: ", "no data", "minBatVoltage"));
    response_->print(fp_.getHtmlTableRowSpan("Max voltage: ", "no data", "maxBatVoltage"));
    response_->print(fp_.getHtmlTableRowSpan("Charged energy:", "no data", "amountChargedEnergy"));
    response_->print(fp_.getHtmlTableRowSpan("Discharged energy:", "no data", "amountDischargedEnergy"));
    response_->print(fp_.getHtmlTableRowSpan("Total Ah drawn:", "no data", "sumApHDrawn"));
    response_->print(fp_.getHtmlTableRowSpan("Last discharged energy:", "no data", "lastDischarge"));
    response_->print(fp_.getHtmlTableRowSpan("Deepest discharged energy:", "no data", "deepestDischarge"));
    response_->print(fp_.getHtmlTableRowSpan("Deepest temperature:", "no data", "deepestTemperatur"));
    response_->print(fp_.getHtmlTableRowSpan("Highest temperature:", "no data", "highestTemperatur"));
    response_->print(fp_.getHtmlTableRowSpan("Time since last full:", "no data", "TimeSinceLastFull"));
    response_->print(fp_.getHtmlTableRowSpan("Charge cycles:", "no data", "numChargeCycles"));
    response_->print(fp_.getHtmlTableRowSpan("Auto syncs:", "no data", "numAutoSyncs"));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(F("<form method='POST' action='/resetstats'>"));
    response_->print(F("<button type='submit' style='margin-top:10px;'>Reset Statistics</button>"));
    response_->print(F("</form>"));
    response_->print(fp_.getHtmlTabContentEnd());

    // === Shunt Tab
    response_->print(fp_.getHtmlTabContentStart("shunt"));
    response_->print(fp_.getHtmlFieldset("Shunt configuration"));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowSpan("Shunt resistance:", String(shuntConfig.ShuntResistance(), 5) + "&#8486;", "shuntResistance"));
    response_->print(fp_.getHtmlTableRowSpan("Shunt max current:", String(shuntConfig.MaxCurrent()) + "A", "maxCurrent"));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.getHtmlTabContentEnd());

    // === Battery Tab
    response_->print(fp_.getHtmlTabContentStart("battery"));
    response_->print(fp_.getHtmlFieldset("Battery configuration"));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowSpan("Type:", String(BatTypeNames[batteryConfig.Type()]), "BatType"));
    response_->print(fp_.getHtmlTableRowSpan("Capacity:", String(batteryConfig.Capacity_Ah()) + "Ah", "battCapacity"));
    response_->print(fp_.getHtmlTableRowSpan("Efficiency:", String(batteryConfig.ChargeEfficiency_Percent()) + "%", "chargeEfficiency"));
    response_->print(fp_.getHtmlTableRowSpan("Min SOC:", String(batteryConfig.MinSoc_Percent()) + "%", "minSoc"));
    float mA_ = batteryFullConfig.TailCurrent_mA();
    float mV_ = batteryFullConfig.FullVoltage_mV();
    response_->print(fp_.getHtmlTableRowSpan("Tail current:", String((mA_ / 1000), 3) + "A", "tailCurrent"));
    response_->print(fp_.getHtmlTableRowSpan("Full voltage:", String((mV_ / 1000), 2) + "V", "fullVoltage"));
    response_->print(fp_.getHtmlTableRowSpan("Full delay:", String(batteryFullConfig.FullDelay_s()) + "s", "fullDelay"));
    response_->print(fp_.getHtmlTableRowSpan("Manufacturer:", String(batteryConfig.Manufacturer()), "BattManufacturer"));
    response_->print(fp_.getHtmlTableRowSpan("Replacment date:", String(batteryConfig.ReplacmentDate()), "BattDate"));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());

    response_->print(fp_.getHtmlFieldset("Set state of charge"));
    response_->print(F("<form method='POST' action='/setsoc'>"));
    response_->print(F("<table border='0' style='width:100%'>"));
    response_->print(F("<tr><td><label for='soc'>New battery SOC:</label></td></tr>"));
    response_->print(F("<tr><td><input type='number' name='soc' id='soc' value='100' min='0' max='100' step='1' style='width:95%' /></td></tr>"));
    response_->print(F("<tr><td><button type='submit' style='margin-top:10px;'>Set SOC</button></td></tr>"));
    response_->print(F("</table>"));
    response_->print(F("</form>"));
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.getHtmlTabContentEnd());

    // === System Tab
    response_->print(fp_.getHtmlTabContentStart("system"));
    response_->print(fp_.getHtmlFieldset("System status"));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowSpan("Number of reboots:", String(RebootManager::getRebootCount()), "rebootCount"));
    response_->print(fp_.getHtmlTableRowSpan("Last reboot reason:", RebootManager::getLastRebootReasonText(), "rebootReason"));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());

    response_->print(fp_.getHtmlFieldset("Network"));
    response_->print(fp_.getHtmlTable());

    uint64_t chipid_ = ESP.getEfuseMac();
    char macStr_[18];
    snprintf(macStr_, sizeof(macStr_), "%02X:%02X:%02X:%02X:%02X:%02X",
        (uint8_t)(chipid_ >> 40), (uint8_t)(chipid_ >> 32),
        (uint8_t)(chipid_ >> 24), (uint8_t)(chipid_ >> 16),
        (uint8_t)(chipid_ >> 8), (uint8_t)(chipid_));
    response_->print(fp_.getHtmlTableRowText("MAC Address:", String(macStr_)));

    String ipAddress_;
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        ipAddress_ = WiFi.softAPIP().toString();
    }
    else {
        ipAddress_ = WiFi.localIP().toString();
    }
    if (ipAddress_ == "0.0.0.0" || ipAddress_.isEmpty()) {
        ipAddress_ = "Not connected";
    }
    response_->print(fp_.getHtmlTableRowText("IP Address:", ipAddress_));

    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());
    response_->print(fp_.getHtmlTabContentEnd());

    response_->print(fp_.addNewLine(2));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowText("<a href='config'>Configuration</a>"));
    response_->print(fp_.getHtmlTableRowText("<a href='webserial'>Sensor monitoring</a>"));
    response_->print(fp_.getHtmlTableRowText(fp_.getHtmlVersion(Version)));
    response_->print(fp_.getHtmlTableEnd());

    response_->print(fp_.getBodyEnd());
    response_->print(fp_.getHtmlEnd());

    response_->addHeader("Server", "ESP Async Web Server");
    request->send(response_);
}
void handlePost(AsyncWebServerRequest* request) {
    if (request->hasParam("reset", true)) {
        Serial.println(F("Resetting to factory defaults..."));

		nmeaConfig.applyDefaultValues();
		shuntConfig.applyDefaultValues();
		batteryFullConfig.applyDefaultValues();
		batteryConfig.applyDefaultValues();

		APModeOfflineParam.applyDefaultValue();
		APModeParam.applyDefaultValue();

        iotWebConf.saveConfig();

        request->send(200, "text/plain", "OK");
    }
    else {
        request->send(400, "text/plain", "Bad Request");
    }
}

void handleReboot(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response_ = request->beginResponse(200, "text/html",
        "<html>"
        "<head>"
        "<meta http-equiv=\"refresh\" content=\"15; url=/\">"
        "<title>Rebooting...</title>"
        "</head>"
        "<body>"
        "Please wait while the device is rebooting...<br>"
        "You will be redirected to the homepage shortly."
        "</body>"
        "</html>");
    request->client()->setNoDelay(true); // Disable Nagle's algorithm so the client gets the response immediately
    request->send(response_);
    ShouldReboot = true;
}

void convertParams() {
    APModeOfflineTime = atoi(APModeOfflineValue);

    ArduinoOTA.setHostname(iotWebConf.getThingName());
}

void configSaved(){ 
  convertParams();
  ParamsChanged = true;
  

  if (!APModeParam.isChecked()) {
	  Serial.println(F("start allways AP mode"));
	  startAPMode = true;
  }
  Serial.println(F("Parameters are saved"));
} 

void connectWifi(const char* ssid, const char* password){
	Serial.println("Connecting to WiFi ...");
    WiFi.begin(ssid, password);
}

iotwebconf::WifiAuthInfo* handleConnectWifiFailure() {
    if (!startAPMode) {
        static iotwebconf::WifiAuthInfo auth_;
        auth_ = iotWebConf.getWifiAuthInfo();
        return &auth_;
	}
    else {
        return nullptr;
    }
}

