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

#include <DNSServer.h>
#include <IotWebConfAsyncTab.h>
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRootTab.h>
#include <RebootManager.h>

#include <N2kTypes.h>

#define SOC_RESPONSE \
"<!DOCTYPE HTML>\
    <html><head>\
        <meta charset=\"UTF-8\">\
        <meta http-equiv=\"refresh\" content=\"2; url=/#battery\">\
        <script type=\"text/javascript\">\
            window.location.href = \"/#battery\"\
        </script>\
        <title>SOC set</title>\
    </head>\
    <body>\
        SOC has been updated<br><a href='/#battery'>Back to Battery</a>\
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

const char html_form_end[] PROGMEM = R"=====(
</br><form action='/reboot' method='get'><button type='submit'>Reboot</button></form>
</br><form action='/' method='get' style='display:inline;'>
  <button type='submit'>Home</button>
  </br>
</form>
</br><form action='/post' method='post' style='display:inline;' onsubmit="event.preventDefault(); postReset();">
  <button type='submit'>Reset to factory defaults</button>
</form>
<script>
function postReset() {
    fetch('/post', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'reset=true'
    })
    .then(response => {
        if (response.ok) { 
            window.location.reload(true);  // Hard reload
        }
    })
    .catch(error => {
        console.error('Reset fehlgeschlagen:', error);
        alert('Reset fehlgeschlagen!');
    });
}
</script>
)=====";

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
        return AsyncTabHtmlFormatProvider::getFormEnd() + String(FPSTR(html_form_end));
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
    String _soc = request->arg("soc");
    _soc.trim();
    if (!_soc.isEmpty()) {
        float _socVal = _soc.toInt() / 100.00;
        batteryStatus.setBatterySoc(_socVal);
		batteryStatus.writeStats();
    }

    request->send(200, "text/html", SOC_RESPONSE);
}

void onResetStatistics(AsyncWebServerRequest* request) {
    batteryStatus.resetStats();
    
    String response_ = F(
        "<!DOCTYPE HTML>"
        "<html><head>"
        "<meta charset=\"UTF-8\">"
        "<meta http-equiv=\"refresh\" content=\"2; url=/#statistics\">"
        "<script type=\"text/javascript\">"
        "window.location.href = \"/#statistics\""
        "</script>"
        "<title>Statistics Reset</title>"
        "</head>"
        "<body>"
        "Statistics have been reset<br>"
        "<a href='/#statistics'>Back to Statistics</a>"
        "</body>"
        "</html>"
    );
    
    request->send(200, "text/html", response_);
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
    virtual String getScriptInner() override {
        String s_ = TabHtmlRootFormatProvider::getScriptInner();
        s_.replace("{millisecond}", "5000");
        
        s_ += F("function updateData(jsonData) {\n");
        s_ += F("   document.getElementById('RSSIValue').innerHTML = jsonData.rssi + \"dBm\" \n");
        s_ += F("   document.getElementById('VoltageValue').innerHTML = jsonData.voltage + \"V\" \n");
        s_ += F("   document.getElementById('CurrentValue').innerHTML = jsonData.current + \"A\" \n");
        s_ += F("   document.getElementById('AverageCurrentValue').innerHTML = jsonData.avgCurrent + \"A\" \n");
        s_ += F("   document.getElementById('SocValue').innerHTML = jsonData.soc + \"%\" \n");
        s_ += F("   document.getElementById('tTgValue').innerHTML = jsonData.ttg \n");
        s_ += F("   document.getElementById('isFullValue').innerHTML = jsonData.isFull \n");
        s_ += F("   document.getElementById('TemperatureValue').innerHTML = jsonData.temperature + \"&deg;C\" \n");
        s_ += F("   document.getElementById('Mode').innerHTML = jsonData.mode \n");
        
        // Statistics Updates
        s_ += F("   document.getElementById('minBatVoltage').innerHTML = jsonData.minBatVoltage + \"V\" \n");
        s_ += F("   document.getElementById('maxBatVoltage').innerHTML = jsonData.maxBatVoltage + \"V\" \n");
        s_ += F("   document.getElementById('amountDischargedEnergy').innerHTML = jsonData.amountDischargedEnergy + \"kWh\" \n");
        s_ += F("   document.getElementById('amountChargedEnergy').innerHTML = jsonData.amountChargedEnergy + \"kWh\" \n");
        s_ += F("   document.getElementById('deepestDischarge').innerHTML = jsonData.deepestDischarge + \"Ah\" \n");
        s_ += F("   document.getElementById('lastDischarge').innerHTML = jsonData.lastDischarge + \"Ah\" \n");
        s_ += F("   document.getElementById('deepestTemperatur').innerHTML = jsonData.deepestTemperatur + \"&deg;C\" \n");
        s_ += F("   document.getElementById('highestTemperatur').innerHTML = jsonData.highestTemperatur + \"&deg;C\" \n");
        s_ += F("   document.getElementById('TimeSinceLastFull').innerHTML = jsonData.TimeSinceLastFull \n");
        s_ += F("   document.getElementById('numChargeCycles').innerHTML = jsonData.numChargeCycles \n");
        s_ += F("   document.getElementById('numAutoSyncs').innerHTML = jsonData.numAutoSyncs \n");
        s_ += F("   document.getElementById('sumApHDrawn').innerHTML = jsonData.sumApHDrawn + \"Ah\" \n");
        s_ += F("}\n");
      
        return s_;
    }
};

void handleRoot(AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)) {
        return;
    }

    AsyncResponseStream* response_ = request->beginResponseStream("text/html", 1024);
    MyHtmlRootFormatProvider fp_;

    // === 5 Tabs erstellen ===
    fp_.addTab("Current", "current");
    fp_.addTab("Statistics", "statistics");
    fp_.addTab("Shunt", "shunt");
    fp_.addTab("Battery", "battery");
    fp_.addTab("System", "system");

    // === TAB 1: Current Values (RSSI + Running Values) ===
    String currentTab_ = "";
    currentTab_ += F("<fieldset align=left style=\"border: 1px solid\"><table border=\"0\" align=\"center\" width=\"100%\">");
    currentTab_ += F("<tr><td align=\"left\">RSSI:</td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr></table></fieldset>");
    
    currentTab_ += fp_.getHtmlFieldset("Running values");
    currentTab_ += fp_.getHtmlTable();
    currentTab_ += fp_.getHtmlTableRowSpan("Voltage: ", "no data", "VoltageValue");
    currentTab_ += fp_.getHtmlTableRowSpan("Current: ", "no data", "CurrentValue");
    currentTab_ += fp_.getHtmlTableRowSpan("Avg current: ", "no data", "AverageCurrentValue");
    currentTab_ += fp_.getHtmlTableRowSpan("State of charge: ", "no data", "SocValue");
    currentTab_ += fp_.getHtmlTableRowSpan("Time to go: ", "no data", "tTgValue");
    currentTab_ += fp_.getHtmlTableRowSpan("Battery full: ", "no data", "isFullValue");
    currentTab_ += fp_.getHtmlTableRowSpan("Temperature: ", "no data", "TemperatureValue");
    currentTab_ += fp_.getHtmlTableRowSpan("Mode: ", "no data", "Mode");
    currentTab_ += fp_.getHtmlTableEnd();
    currentTab_ += fp_.getHtmlFieldsetEnd();
    fp_.addString(currentTab_, "current");

    // === TAB 2: Statistics ===
    String statsTab_ = "";
    statsTab_ += fp_.getHtmlFieldset("Statistics");
    statsTab_ += fp_.getHtmlTable();
    statsTab_ += fp_.getHtmlTableRowSpan("Min voltage: ", "no data", "minBatVoltage");
    statsTab_ += fp_.getHtmlTableRowSpan("Max voltage: ", "no data", "maxBatVoltage");
    statsTab_ += fp_.getHtmlTableRowSpan("Charged energy:", "no data", "amountChargedEnergy");
    statsTab_ += fp_.getHtmlTableRowSpan("Discharged energy:", "no data", "amountDischargedEnergy");
    statsTab_ += fp_.getHtmlTableRowSpan("Total Ah drawn:", "no data", "sumApHDrawn");
    statsTab_ += fp_.getHtmlTableRowSpan("Last discharged energy:", "no data", "lastDischarge");
    statsTab_ += fp_.getHtmlTableRowSpan("Deepest discharged energy:", "no data", "deepestDischarge");
    statsTab_ += fp_.getHtmlTableRowSpan("Deepest temperature:", "no data", "deepestTemperatur");
    statsTab_ += fp_.getHtmlTableRowSpan("Highest temperature:", "no data", "highestTemperatur");
    statsTab_ += fp_.getHtmlTableRowSpan("Time since last full:", "no data", "TimeSinceLastFull");
    statsTab_ += fp_.getHtmlTableRowSpan("Charge cycles:", "no data", "numChargeCycles");
    statsTab_ += fp_.getHtmlTableRowSpan("Auto syncs:", "no data", "numAutoSyncs");
    statsTab_ += fp_.getHtmlTableEnd();
    statsTab_ += fp_.getHtmlFieldsetEnd();
    
    // Reset Button
    statsTab_ += F("<form method='POST' action='/resetstats'>");
    statsTab_ += F("<button type='submit' style='margin-top:10px;'>Reset Statistics</button>");
    statsTab_ += F("</form>");
    
    fp_.addString(statsTab_, "statistics");

    // === TAB 3: Shunt Configuration ===
    String shuntTab_ = "";
    shuntTab_ += fp_.getHtmlFieldset("Shunt configuration");
    shuntTab_ += fp_.getHtmlTable();
    shuntTab_ += fp_.getHtmlTableRowSpan("Shunt resistance:", String(shuntConfig.ShuntResistance(), 5) + "&#8486;", "shuntResistance");
    shuntTab_ += fp_.getHtmlTableRowSpan("Shunt max current:", String(shuntConfig.MaxCurrent()) + "A", "maxCurrent");
    shuntTab_ += fp_.getHtmlTableEnd();
    shuntTab_ += fp_.getHtmlFieldsetEnd();
    fp_.addString(shuntTab_, "shunt");

    // === TAB 4: Battery Configuration ===
    String batteryTab_ = "";
    batteryTab_ += fp_.getHtmlFieldset("Battery configuration");
    batteryTab_ += fp_.getHtmlTable();
    batteryTab_ += fp_.getHtmlTableRowSpan("Type:", String(BatTypeNames[batteryConfig.Type()]), "BatType");
    batteryTab_ += fp_.getHtmlTableRowSpan("Capacity:", String(batteryConfig.Capacity_Ah()) + "Ah", "battCapacity");
    batteryTab_ += fp_.getHtmlTableRowSpan("Efficiency:", String(batteryConfig.ChargeEfficiency_Percent()) + "%", "chargeEfficiency");
    batteryTab_ += fp_.getHtmlTableRowSpan("Min SOC:", String(batteryConfig.MinSoc_Percent()) + "%", "minSoc");
    float mA_ = batteryFullConfig.TailCurrent_mA();
    float mV_ = batteryFullConfig.FullVoltage_mV();
    batteryTab_ += fp_.getHtmlTableRowSpan("Tail current:", String((mA_ / 1000), 3) + "A", "tailCurrent");
    batteryTab_ += fp_.getHtmlTableRowSpan("Full voltage:", String((mV_ / 1000), 2) + "V", "fullVoltage");
    batteryTab_ += fp_.getHtmlTableRowSpan("Full delay:", String(batteryFullConfig.FullDelay_s()) + "s", "fullDelay");
    batteryTab_ += fp_.getHtmlTableRowSpan("Manufacturer:", String(batteryConfig.Manufacturer()), "BattManufacturer");
    batteryTab_ += fp_.getHtmlTableRowSpan("Replacment date:", String(batteryConfig.ReplacmentDate()), "BattDate");
    batteryTab_ += fp_.getHtmlTableEnd();
    batteryTab_ += fp_.getHtmlFieldsetEnd();

    // Set SOC Form
    batteryTab_ += fp_.getHtmlFieldset("Set state of charge");
    batteryTab_ += F("<form method='POST' action='/setsoc'>");
    batteryTab_ += F("<table border='0' style='width:100%'>");
    batteryTab_ += F("<tr><td><label for='soc'>New battery SOC:</label></td></tr>");
    batteryTab_ += F("<tr><td><input type='number' name='soc' id='soc' value='100' min='0' max='100' step='1' style='width:95%' /></td></tr>");
    batteryTab_ += F("<tr><td><button type='submit' style='margin-top:10px;'>Set SOC</button></td></tr>");
    batteryTab_ += F("</table>");
    batteryTab_ += F("</form>");
    batteryTab_ += fp_.getHtmlFieldsetEnd();

    fp_.addString(batteryTab_, "battery");

    // === TAB 5: System (Reboot Info + Network) ===
    String systemTab_ = "";
    
    systemTab_ += fp_.getHtmlFieldset("System status");
    systemTab_ += fp_.getHtmlTable();
    systemTab_ += fp_.getHtmlTableRowSpan("Number of reboots:", String(RebootManager::getRebootCount()), "rebootCount");
    systemTab_ += fp_.getHtmlTableRowSpan("Last reboot reason:", RebootManager::getLastRebootReasonText(), "rebootReason");
    systemTab_ += fp_.getHtmlTableEnd();
    systemTab_ += fp_.getHtmlFieldsetEnd();

    systemTab_ += fp_.getHtmlFieldset("Network");
    systemTab_ += fp_.getHtmlTable();
    systemTab_ += fp_.getHtmlTableRowText("MAC Address:", WiFi.macAddress());
    systemTab_ += fp_.getHtmlTableRowText("IP Address:", WiFi.localIP().toString().c_str());
    systemTab_ += fp_.getHtmlTableEnd();
    systemTab_ += fp_.getHtmlFieldsetEnd();

    fp_.addString(systemTab_, "system");

    // === HTML Ausgabe ===
    response_->print(fp_.getHtmlHead(iotWebConf.getThingName()));
    response_->print(fp_.getIconLinks());
    response_->print(fp_.getHtmlStyle());
    response_->print(fp_.getHtmlHeadEnd());
    response_->print(fp_.getHtmlScript());
    response_->print(fp_.getHtmlTabs());

    // Footer
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

