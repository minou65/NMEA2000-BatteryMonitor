// #define DEBUG_WIFI(m) SERIAL_DBG.print(m)

#include <Arduino.h>
#include <WiFi.h>
#include <neotimer.h>

#include <DNSServer.h>
#include <IotWebConfAsyncTab.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRootTab.h>
#include <ArduinoJson.h>

#include <EventLogger.h>
#include <RebootManager.h>
#include <N2kTypes.h>

#include "common.h"
#include "html.h"
#include "webHandling.h"
#include "statusHandling.h"
#include "favicon.h"
#include "BatteryConfig.h"
#include "NMEAConfig.h"
#include "APModeConfig.h"

// -- Method declarations.
void handleSetRuntime(AsyncWebServerRequest* request);
void onSetSoc(AsyncWebServerRequest* request);
void onResetStatistics(AsyncWebServerRequest* request);
void handleData(AsyncWebServerRequest* request);
void handleRoot(AsyncWebServerRequest* request);
void handlePost(AsyncWebServerRequest* request);
void handleWebCommand(const String& command);
void handleReboot(AsyncWebServerRequest* request);
void handleConfigSavedPage(iotwebconf::WebRequestWrapper* webRequestWrapper);
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

class CustomHtmlFormatProvider : public AsyncTabHtmlFormatProvider {
public:
    CustomHtmlFormatProvider(std::vector<AsyncTabInfo>* tabs, int minWidth = 500, int maxWidth = 600)
        : AsyncTabHtmlFormatProvider(tabs, minWidth, maxWidth) {
    }

protected:
    String getFormEnd() override {
        String s_ = AsyncTabHtmlFormatProvider::getFormEnd();
        s_ += FPSTR(html_form_end);
        return s_;
    }
};
CustomHtmlFormatProvider* customHtmlFormatProvider = nullptr;

void wifiStoreConfig() {
    iotWebConf.saveConfig();
}

void wifiConnected(){
    WiFi.setSleep(false);
    LOG_INFO("WiFi modem sleep disabled");  
}

void wifiSetup() {
    iotWebConf.setStatusPin(STATUS_PIN,ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);

    // === TABS KONFIGURIEREN ===
    // System Tab soll an letzter Stelle sein
    iotWebConf.setSystemTabPosition(-1);  // -1 = letzte Position
    iotWebConf.setSystemTabName("System");

    iotWebConf.addParameterGroup(&apModeConfig, "System");

    // Battery Tab
    iotWebConf.addParameterGroup(&batteryConfig, "Battery");
    iotWebConf.addParameterGroup(&batteryFullConfig, "Battery");

    // Shunt Tab
    iotWebConf.addParameterGroup(&shuntConfig, "Shunt");

    // NMEA Tab
    iotWebConf.addParameterGroup(&nmeaConfig, "NMEA");


    // CustomHtmlFormatProvider mit Tabs-Vektor erstellen
    auto customProvider = new CustomHtmlFormatProvider(iotWebConf.getTabsVector());
    customProvider->setContainerWidth(700, 700);
    iotWebConf.setHtmlFormatProvider(customProvider);

    iotWebConf.setupUpdateServer(
        [](const char* updatePath) {
            AsyncUpdater.onBegin([](const String& message) {
                LOG_INFO("%s", message.c_str());
                });
            AsyncUpdater.onEnd([](const String& message) { LOG_OK("%s", message.c_str()); });
            AsyncUpdater.OnError([](const String& message) {
                LOG_ERROR("%s", message.c_str());
                });
            AsyncUpdater.onProgress([](const String& message) { LOG_INFO("%s", message.c_str()); });
            AsyncUpdater.onLog([](const String& message) { WEB_LOG_INFO("%s", message.c_str()); });
            AsyncUpdater.setup(&server, updatePath);
        },
        [](const char* userName, char* password) { AsyncUpdater.updateCredentials(userName, password); });
  
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);
    iotWebConf.setConfigSavedPage(&handleConfigSavedPage);

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

  // EventLogger Web initialisieren
    bool webOk_ = eventLogger.beginWeb(&server, "/events");
    if (!webOk_) {
        eventLogger.log("EventLogger.beginWeb failed", EventLogger::Status::ERROR, 1);
    }

    eventLogger.setCommandCallback([](const String& command_) {
        handleWebCommand(command_);
        });

    server.onNotFound([](AsyncWebServerRequest* request) {
            AsyncWebRequestWrapper asyncWebRequestWrapper(request);
            iotWebConf.handleNotFound(&asyncWebRequestWrapper);
        }
    );

    if (APModeOfflineTime > 0) {
        APModeTimer.start(APModeOfflineTime * 60 * 1000);
    }
}

void wifiLoop() {

    if (SaveParams) {
        eventLogger.log(F("Parameters are changed,save them"));

        iotWebConf.saveConfig();
        SaveParams = false;
    }

    if ((iotWebConf.getState() == iotwebconf::OnLine) && (apModeConfig.isStartOnlyAtBoot()) && startAPMode) {
        eventLogger.log(F("start AP mode only at boot sequence"));
        startAPMode = false;
    }

    if (APModeTimer.done()) {
        eventLogger.log(F("AP mode offline time reached"));
        iotWebConf.goOffLine();
        APModeTimer.stop();
    }

    if (ShouldReboot || AsyncUpdater.isFinished()) {
        eventLogger.log(F("Rebooting..."));
        batteryStatus.writeStats();
        delay(1000);
        ESP.restart();
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

    LOG_INFO("SOC set to %d%%", socInt_);

    request->send_P(200, "text/html", html_soc_response);
}

void onResetStatistics(AsyncWebServerRequest* request) {
    batteryStatus.resetStats();

    LOG_INFO("Statistics reset");

    request->send_P(200, "text/html", html_stats_reset_response);
}

void handleData(AsyncWebServerRequest* request) {
    JsonDocument doc_;
    const Statistics& stats_ = batteryStatus.statistics();
    const auto rounded_ = [](double value_, double scale_) {
        return round(value_ * scale_) / scale_;
    };

    doc_["rssi"] = WiFi.RSSI();
    doc_["voltage"] = rounded_(batteryStatus.voltage(), 100);
    doc_["current"] = rounded_(batteryStatus.current(), 100);
    doc_["avgCurrent"] = rounded_(batteryStatus.averageCurrent(), 100);
    doc_["soc"] = rounded_(batteryStatus.soc() * 100, 10);

    char timeToGo_[24];
    unsigned int ttg_ = batteryStatus.tTg();
    if (ttg_ == UINT32_MAX) {
        ttg_ = 0;
    }
    snprintf(timeToGo_, sizeof(timeToGo_), "%02u:%02u", ttg_ / 3600, (ttg_ % 3600) / 60);
    doc_["ttg"] = timeToGo_;
    doc_["mode"] = batteryStatus.current() >= 0 ? "charging" : "discharging";
    doc_["isFull"] = batteryStatus.isFull() ? "true" : "false";
    doc_["temperature"] = rounded_(batteryStatus.temperatur(), 10);
    doc_["batteryType"] = BatTypeNames[batteryConfig.Type()];
    doc_["batteryVoltage"] = BatNomVoltNames[batteryConfig.NomVolt()];
    doc_["batteryChemistry"] = BatChemNames[batteryConfig.Chemistry()];
    doc_["capacity"] = batteryConfig.Capacity_Ah();
    doc_["chargeEfficiency"] = batteryConfig.ChargeEfficiency_Percent();
    doc_["minSoc"] = batteryConfig.MinSoc_Percent();
    doc_["tailCurrent"] = rounded_(batteryFullConfig.TailCurrent_mA() / 1000.00f, 1000);
    doc_["fullVoltage"] = rounded_(batteryFullConfig.FullVoltage_mV() / 1000.00f, 100);
    doc_["fullDelay"] = batteryFullConfig.FullDelay_s();
    doc_["currentThreshold"] = rounded_(batteryFullConfig.CurrentThreshold_A(), 1000);
    doc_["shuntResistance"] = rounded_(shuntConfig.ShuntResistance(), 100000);
    doc_["maxCurrent"] = rounded_(shuntConfig.MaxCurrent(), 100);
    doc_["voltageCalibrationFactor"] = rounded_(shuntConfig.VoltageCalibrationFactor(), 100);
    doc_["currentCalibrationFactor"] = rounded_(shuntConfig.CurrentCalibrationFactor(), 100);

    doc_["consumedAh"] = rounded_(stats_.consumedAs / 3600.00f, 1000);
    doc_["consumedAs"] = rounded_(stats_.consumedAs, 100);
    doc_["deepestDischarge"] = rounded_(stats_.deepestDischarge / 1000.00f, 1000);
    doc_["lastDischarge"] = rounded_(stats_.lastDischarge / 1000.00f, 1000);
    doc_["averageDischarge"] = rounded_(stats_.averageDischarge / 1000.00f, 1000);
    doc_["numChargeCycles"] = stats_.numChargeCycles;
    doc_["numFullDischarge"] = stats_.numFullDischarge;
    doc_["sumApHDrawn"] = rounded_(stats_.sumApHDrawn, 1000);
    doc_["minBatVoltage"] = rounded_(stats_.minBatVoltage / 1000.00f, 100);
    doc_["maxBatVoltage"] = rounded_(stats_.maxBatVoltage / 1000.00f, 100);

    char sinceFull_[24];
    int seconds_ = stats_.secsSinceLastFull;
    if (seconds_ == -1) {
        seconds_ = 0;
    }
    snprintf(sinceFull_, sizeof(sinceFull_), "%02d:%02d:%02d",
        seconds_ / 3600, (seconds_ % 3600) / 60, seconds_ % 60);
    doc_["TimeSinceLastFull"] = sinceFull_;

    doc_["secsSinceLastFull"] = stats_.secsSinceLastFull;
    doc_["numAutoSyncs"] = stats_.numAutoSyncs;
    doc_["numLowVoltageAlarms"] = stats_.numLowVoltageAlarms;
    doc_["numHighVoltageAlarms"] = stats_.numHighVoltageAlarms;
    doc_["amountDischargedEnergy"] = rounded_(stats_.amountDischargedEnergy, 1000);
    doc_["amountChargedEnergy"] = rounded_(stats_.amountChargedEnergy, 1000);
    doc_["deepestTemperatur"] = rounded_(stats_.deepestTemperatur, 100);
    doc_["highestTemperatur"] = rounded_(stats_.highestTemperatur, 100);

    String json_;
    size_t length_ = measureJson(doc_);
    if (doc_.overflowed() || !json_.reserve(length_) || serializeJson(doc_, json_) != length_) {
        LOG_ERROR("Failed to serialize battery data");
        request->send(500, "text/plain", "JSON serialization failed");
        return;
    }
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

    AsyncResponseStream* response_ = request->beginResponseStream("text/html", 8192);
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
    response_->print(fp_.getHtmlTableRowText("Go to <a href = 'config'>configure page</a> to change configuration."));
    response_->print(fp_.getHtmlTableRowText("Go to <a href = 'events'>event log</a> to view the event history."));
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

		apModeConfig.applyDefaultValues();
        iotWebConf.saveConfig();

        LOG_OK("Factory defaults applied");

        request->send(200, "text/plain", "OK");
    }
    else {
        request->send(400, "text/plain", "Bad Request");
    }
}

void handleWebCommand(const String& command) {
    String command_ = command;
    command_.trim();
    command_.toLowerCase();

    if (command_.length() == 0) {
        return;
    }

    if (command_ == "help") {
        WEB_LOG_INFO_AT(0, "=== Available Commands ===");
        WEB_LOG_INFO_AT(1, "help         - Show this help");
        WEB_LOG_INFO_AT(1, "reboot       - Reboot the device (also: restart)");
        WEB_LOG_INFO_AT(1, "clear        - Clear event buffer");
        WEB_LOG_INFO_AT(1, "status       - Show system status");
        WEB_LOG_INFO_AT(1, "wifi         - Show WiFi info");
        WEB_LOG_INFO_AT(1, "sensor       - Show sensor values");
        WEB_LOG_INFO_AT(1, "history      - Show all buffered events");
        WEB_LOG_INFO_AT(1, "verbose      - Toggle verbose logging");
        WEB_LOG_INFO_AT(1, "verbose=Ah   - Toggle Ah counter logging");
        WEB_LOG_INFO_AT(1, "verbose=SOC  - Toggle SOC logging");
        WEB_LOG_INFO_AT(1, "verbose=TTG  - Toggle TTG logging");
        WEB_LOG_INFO_AT(1, "verbose=Cons - Toggle consumption logging");
    }
    else if (command_ == "reboot" || command_ == "restart") {
        WEB_LOG_INFO_AT(0, "Rebooting device...");
        eventLogger.log("Reboot requested via web command");
        ShouldReboot = true;
    }
    else if (command_ == "clear") {
        eventLogger.clear();
        WEB_LOG_INFO_AT(0, "Event buffer cleared");
    }
    else if (command_ == "status") {
        WEB_LOG_INFO_AT(0, "=== System Status ===");
        WEB_LOG_INFO_AT(1, "Version: %s", Version);
        WEB_LOG_INFO_AT(1, "WiFi SSID: %s", WiFi.SSID().c_str());
        WEB_LOG_INFO_AT(1, "WiFi RSSI: %d dBm", WiFi.RSSI());
        WEB_LOG_INFO_AT(1, "IP Address: %s", WiFi.localIP().toString().c_str());
        unsigned long uptime_ = millis() / 1000;
        unsigned long days_ = uptime_ / 86400;
        unsigned long hours_ = (uptime_ % 86400) / 3600;
        unsigned long minutes_ = (uptime_ % 3600) / 60;
        unsigned long seconds_ = uptime_ % 60;

        WEB_LOG_INFO_AT(1, "Uptime: %lud %02lu:%02lu:%02lu", days_, hours_, minutes_, seconds_);
        WEB_LOG_INFO_AT(1, "Free Heap: %u bytes", ESP.getFreeHeap());
        WEB_LOG_INFO_AT(1, "Events in buffer: %u/%u",
            static_cast<unsigned int>(eventLogger.count()), static_cast<unsigned int>(EVENT_BUFFER_SIZE));
        WEB_LOG_INFO_AT(0, "=== Debug Flags ===");
        WEB_LOG_INFO_AT(1, "Debug Mode: %s", debugMode ? "ON" : "OFF");
        WEB_LOG_INFO_AT(1, "Ah Counter: %s", debugAhCounter ? "ON" : "OFF");
        WEB_LOG_INFO_AT(1, "SOC: %s", debugSOC ? "ON" : "OFF");
        WEB_LOG_INFO_AT(1, "TTG: %s", debugTTG ? "ON" : "OFF");
        WEB_LOG_INFO_AT(1, "Consumption: %s", debugConsumption ? "ON" : "OFF");
    }
    else if (command_ == "wifi") {
        WEB_LOG_INFO_AT(0, "=== WiFi Info ===");
        WEB_LOG_INFO_AT(1, "SSID: %s", WiFi.SSID().c_str());
        WEB_LOG_INFO_AT(1, "IP: %s", WiFi.localIP().toString().c_str());
        WEB_LOG_INFO_AT(1, "MAC: %s", WiFi.macAddress().c_str());
        WEB_LOG_INFO_AT(1, "RSSI: %d dBm", WiFi.RSSI());
    }
    else if (command_ == "sensor") {
        WEB_LOG_INFO_AT(0, "=== Sensor Values ===");
        WEB_LOG_INFO_AT(1, "INA226: %s", INA226Initialized ? "initialized" : "not initialized");
        WEB_LOG_INFO_AT(1, "Voltage: %.2f V", batteryStatus.voltage());
        WEB_LOG_INFO_AT(1, "Current: %.2f A", batteryStatus.current());
        WEB_LOG_INFO_AT(1, "Temperature: %.2f C", batteryStatus.temperatur());
        WEB_LOG_INFO_AT(1, "SOC: %.1f %%", batteryStatus.soc() * 100);
    }
    else if (command_ == "history") {
        auto events_ = eventLogger.getEvents();
        WEB_LOG_INFO_AT(0, "=== Event History (%u events) ===", static_cast<unsigned int>(events_.size()));
        if (events_.empty()) {
            WEB_LOG_INFO_AT(1, "No events in buffer");
        }
        else {
            for (const auto& event_ : events_) {
                WEB_LOG_INFO_AT(1, "%s", eventLogger.formatEvent(event_).c_str());
            }
        }
    }
    else if (command_ == "verbose") {
        debugMode = !debugMode;
        WEB_LOG_INFO_AT(0, "Verbose logging is now %s", debugMode ? "enabled" : "disabled");
    }
    else if (command_.startsWith("verbose=")) {
        String param_ = command_.substring(8);

        if (param_ == "ah") {
            debugAhCounter = !debugAhCounter;
            WEB_LOG_INFO_AT(0, "Ah counter logging is now %s", debugAhCounter ? "enabled" : "disabled");
        }
        else if (param_ == "soc") {
            debugSOC = !debugSOC;
            WEB_LOG_INFO_AT(0, "SOC logging is now %s", debugSOC ? "enabled" : "disabled");
        }
        else if (param_ == "ttg") {
            debugTTG = !debugTTG;
            WEB_LOG_INFO_AT(0, "TTG logging is now %s", debugTTG ? "enabled" : "disabled");
        }
        else if (param_ == "cons") {
            debugConsumption = !debugConsumption;
            WEB_LOG_INFO_AT(0, "Consumption logging is now %s", debugConsumption ? "enabled" : "disabled");
        }
        else {
            WEB_LOG_INFO_AT(0, "Unknown verbose parameter: %s", param_.c_str());
            WEB_LOG_INFO_AT(0, "Valid parameters: Ah, SOC, TTG, Cons");
        }
    }
    else {
        WEB_LOG_INFO_AT(0, "Unknown command: %s", command_.c_str());
        WEB_LOG_INFO_AT(0, "Type 'help' for available commands");
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

void handleConfigSavedPage(iotwebconf::WebRequestWrapper* webRequestWrapper) {
    webRequestWrapper->sendHeader("Location", "/");
    webRequestWrapper->send(302, "text/plain", "");
}

void convertParams() {
    APModeOfflineTime = apModeConfig.getOfflineTime();
}

void configSaved(){ 
  convertParams();
  ParamsChanged = true;
  
  if (!apModeConfig.isStartOnlyAtBoot()) {
      startAPMode = true;
      LOG_INFO("AP mode allowed on WiFi connection failure");
  }

  LOG_OK("Parameters saved successfully");
} 

void connectWifi(const char* ssid, const char* password){
	LOG_INFO("Connecting to WiFi with SSID: %s", ssid);
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

