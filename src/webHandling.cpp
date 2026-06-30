// #define DEBUG_WIFI(m) SERIAL_DBG.print(m)

#include <Arduino.h>
#include <ArduinoOTA.h>

#if defined(ESP32)
#include <WiFi.h>
// #include <esp_wifi.h>
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
#include <IotWebConfAsyncUpdateServer.h>
#include <IotWebRoot.h>
#include <RebootManager.h>

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
void onResetStatistics(AsyncWebServerRequest* request);
void onProgress(size_t prg, size_t sz);
void handleData(AsyncWebServerRequest* request);
void handleRoot(AsyncWebServerRequest* request);
void handleStatistics(AsyncWebServerRequest* request);
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

// -- We can add a legend to the separator
AsyncIotWebConf iotWebConf(CustomName, &dnsServer, &asyncWebServerWrapper, wifiInitialApPassword, CONFIG_VERSION);
NMEAConfig nmeaConfig  = NMEAConfig();
BatteryConfig batteryConfig = BatteryConfig();
BatteryFullConfig batteryFullConfig = BatteryFullConfig();
ShuntConfig shuntConfig = ShuntConfig();

char APModeValue[STRING_LEN];
iotwebconf::CheckboxParameter APModeParam = iotwebconf::CheckboxParameter("start AP only at boot sequence", "APModeID", APModeValue, STRING_LEN, false);

char APModeOfflineValue[STRING_LEN];
iotwebconf::NumberParameter APModeOfflineParam = iotwebconf::NumberParameter("AP offline mode after (minutes)", "APModeOffline", APModeOfflineValue, NUMBER_LEN, "0", "0..30", "min='0' max='30', step='1'");



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

     
    iotWebConf.setStatusPin(STATUS_PIN,ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);
    iotWebConf.setHtmlFormatProvider(&customHtmlFormatProvider);

    iotWebConf.addParameterGroup(&nmeaConfig);
    iotWebConf.addParameterGroup(&shuntConfig);
    iotWebConf.addParameterGroup(&batteryConfig);
    iotWebConf.addParameterGroup(&batteryFullConfig);

    iotWebConf.addSystemParameter(&APModeParam);
    iotWebConf.addSystemParameter(&APModeOfflineParam);

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
    server.on("/stats", HTTP_GET, [](AsyncWebServerRequest* request) { handleStatistics(request); });
    
    server.on("/config", HTTP_ANY, [](AsyncWebServerRequest* request) {
        auto* asyncWebRequestWrapper = new AsyncWebRequestWrapper(request);
        iotWebConf.handleConfig(asyncWebRequestWrapper);
        }
    );

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
            AsyncWebServerResponse* response = request->beginResponse_P(200, "image/x-icon", favicon_ico, sizeof(favicon_ico));
            request->send(response);
        }
    );

    server.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest* request) {
            AsyncWebServerResponse* response = request->beginResponse_P(200, "image/png", favicon_ico, sizeof(favicon_ico));
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
    server.on("/resetstats", HTTP_POST, [](AsyncWebServerRequest* request) {
            onResetStatistics(request);
        }
    );
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) { handleData(request); });

    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "text/html",
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
        request->send(response);
		ShouldReboot = true;
        }
    );



    server.onNotFound([](AsyncWebServerRequest* request) {
            AsyncWebRequestWrapper asyncWebRequestWrapper(request);
            iotWebConf.handleNotFound(&asyncWebRequestWrapper);
        }
    );

    WebSerial.begin(&server, "/webserial");

    if (APModeOfflineTime > 0) {
        APModeTimer.start(APModeOfflineTime * 60 * 1000);
    }

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
	json_ += "\"deepestTemperatur\":" + String(batteryStatus.statistics().deepestTemperatur, 2) + ","; //°C
	json_ += "\"highestTemperatur\":" + String(batteryStatus.statistics().highestTemperatur, 2) + ""; //°C

	json_ += "}";
	request->send(200, "application/json", json_);
}

class MyHtmlRootFormatProvider : public HtmlRootFormatProvider {
protected:
    virtual String getScriptInner() {
        String s_ = HtmlRootFormatProvider::getScriptInner();
        s_.replace("{millisecond}", "5000");
        s_ += F("function updateData(jsonData) {\n");
        s_ += F("   document.getElementById('RSSIValue').innerHTML = jsonData.rssi + \"dBm\" \n");
		s_ += F("   document.getElementById('VoltageValue').innerHTML = jsonData.voltage + \"V\" \n");
		s_ += F("   document.getElementById('CurrentValue').innerHTML = jsonData.current + \"A\" \n");
		s_ += F("   document.getElementById('AverageCurrentValue').innerHTML = jsonData.avgCurrent + \"A\" \n");
		s_ += F("   document.getElementById('SocValue').innerHTML = jsonData.soc + \"%\" \n");
		s_ += F("   document.getElementById('tTgValue').innerHTML = jsonData.tTg + \"h\" \n");
		s_ += F("   document.getElementById('isFullValue').innerHTML = jsonData.isFull \n");
		s_ += F("   document.getElementById('TemperatureValue').innerHTML = jsonData.temperature + \"&deg;C\" \n");
		s_ += F("   document.getElementById('Mode').innerHTML = jsonData.mode \n");
        s_ += F("}\n");

        return s_;
    }
};


uint32_t rebootCount = 0;
uint32_t lastRebootReason = 0;

void handleRoot(AsyncWebServerRequest* request) {
    AsyncWebRequestWrapper asyncWebRequestWrapper(request);
    if (iotWebConf.handleCaptivePortal(&asyncWebRequestWrapper)) {
        return;
    }

    AsyncResponseStream* response_ = request->beginResponseStream("text/html", 1024);
    MyHtmlRootFormatProvider fp_;

	response_->print(fp_.getHtmlHead(iotWebConf.getThingName()));
    response_->print(F("<link rel=\"icon\" type=\"image/png\" sizes=\"96x96\" href=\"/apple-touch-icon.png\">\n"));
    response_->print(F("<link rel=\"apple-touch-icon\" sizes=\"96x96\" href=\"/apple-touch-icon.png\">\n"));

	response_->print(fp_.getHtmlStyle());
	response_->print(fp_.getHtmlHeadEnd());
	response_->print(fp_.getHtmlScript());
	response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRow());
    response_->print(fp_.getHtmlTableCol());

	response_->print(String(F("<fieldset align=left style=\"border: 1px solid\">\n")));
	response_->print(String(F("<table border=\"0\" align=\"center\" width=\"100%\">\n")));
	response_->print(String(F("<tr><td align=\"left\"> </td></td><td align=\"right\"><span id=\"RSSIValue\">no data</span></td></tr>\n")));
	response_->print(fp_.getHtmlTableEnd());
	response_->print(fp_.getHtmlFieldsetEnd());

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

	response_->print(fp_.getHtmlFieldset("Shunt configuration"));
	response_->print(fp_.getHtmlTable());
	response_->print(fp_.getHtmlTableRowSpan("Shunt resistance:", String(shuntConfig.ShuntResistance(), 5) + "&#8486;", "shuntResistance"));
	response_->print(fp_.getHtmlTableRowSpan("Shunt max current:", String(shuntConfig.MaxCurrent()) + "A", "maxCurrent"));
	response_->print(fp_.getHtmlTableEnd());
	response_->print(fp_.getHtmlFieldsetEnd());

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

    response_->print(fp_.getHtmlFieldset("System status"));
    response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowSpan("Number of reboots:", String(RebootManager::getRebootCount()), "rebootCount"));
    response_->print(fp_.getHtmlTableRowSpan("Last reboot reason:", RebootManager::getLastRebootReasonText(), "rebootReason"));
    response_->print(fp_.getHtmlTableEnd());
    response_->print(fp_.getHtmlFieldsetEnd());

	response_->print(fp_.getHtmlFieldset("Network"));
	response_->print(fp_.getHtmlTable());
	response_->print(fp_.getHtmlTableRowText("MAC Address:", WiFi.macAddress()));
	response_->print(fp_.getHtmlTableRowText("IP Address:", WiFi.localIP().toString().c_str()));
	response_->print(fp_.getHtmlTableEnd());
	response_->print(fp_.getHtmlFieldsetEnd());

	response_->print(fp_.addNewLine(2));
    
	response_->print(fp_.getHtmlTable());
    response_->print(fp_.getHtmlTableRowText("<a href = 'setruntime'>Set state of charge</a>"));
    response_->print(fp_.getHtmlTableRowText("<a href = 'stats'>Statistics</a>"));
	response_->print(fp_.getHtmlTableRowText("<a href = 'config'>Configuration</a>"));
    response_->print(fp_.getHtmlTableRowText("<a href = 'webserial'>Sensor monitoring</a>"));
	
    response_->print(fp_.getHtmlTableRowText(fp_.getHtmlVersion(Version)));
	response_->print(fp_.getHtmlTableEnd());

    response_->print(fp_.getHtmlTableColEnd());
    response_->print(fp_.getHtmlTableRowEnd());
	response_->print(fp_.getHtmlTableEnd());
	response_->print(fp_.getHtmlEnd());

    response_->addHeader("Server", "ESP Async Web Server");
    request->send(response_);
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

        //_s += F("   document.getElementById('consumedAh').innerHTML = jsonData.consumedAh + \"Ah\" \n");
        //_s += F("   document.getElementById('averageDischarge').innerHTML = jsonData.averageDischarge + \"Ah\" \n");
        _s += F("   document.getElementById('deepestDischarge').innerHTML = jsonData.deepestDischarge + \"Ah\" \n");
        _s += F("   document.getElementById('lastDischarge').innerHTML = jsonData.lastDischarge + \"Ah\" \n");

        _s += F("   document.getElementById('deepestTemperatur').innerHTML = jsonData.deepestTemperatur + \"&deg;C\" \n");
        _s += F("   document.getElementById('highestTemperatur').innerHTML = jsonData.highestTemperatur + \"&deg;C\" \n");
        _s += F("   document.getElementById('TimeSinceLastFull').innerHTML = jsonData.TimeSinceLastFull\n");

        _s += F("   document.getElementById('numChargeCycles').innerHTML = jsonData.numChargeCycles \n");
        _s += F("   document.getElementById('numAutoSyncs').innerHTML = jsonData.numAutoSyncs \n");

        _s += F("   document.getElementById('sumApHDrawn').innerHTML = jsonData.sumApHDrawn + \"Ah\" \n");

        _s += F("}\n");

        return _s;
    }
};

void handleStatistics(AsyncWebServerRequest* request) {
    std::string _content;
    MyHtmlRootFormatProviderStatistics _fp;

    _content += _fp.getHtmlHead(iotWebConf.getThingName()).c_str();
    _content += _fp.getHtmlStyle().c_str();
    _content += _fp.getHtmlHeadEnd().c_str();
    _content += _fp.getHtmlScript().c_str();
    _content += _fp.getHtmlTable().c_str();
    _content += _fp.getHtmlTableRow().c_str();
    _content += _fp.getHtmlTableCol().c_str();

    _content += _fp.getHtmlFieldset("Statistics").c_str();
    _content += _fp.getHtmlTable().c_str();

    _content += _fp.getHtmlTableRowSpan("min Volt: ", "no data", "minBatVoltage").c_str();
    _content += _fp.getHtmlTableRowSpan("max Volt: ", "no data", "maxBatVoltage").c_str();

    //_content += _fp.getHtmlTableRowSpan("Consumed Ah: ", "no data", "consumedAh").c_str();
    _content += _fp.getHtmlTableRowSpan("Charged energy:", "no data", "amountChargedEnergy").c_str();
    _content += _fp.getHtmlTableRowSpan("Discharged energy:", "no data", "amountDischargedEnergy").c_str();
    //_content += _fp.getHtmlTableRowSpan("Avg. discharged energy:", "no data", "averageDischarge").c_str();
    _content += _fp.getHtmlTableRowSpan("Total Ah drawn:", "no data", "sumApHDrawn").c_str();
    
    _content += _fp.getHtmlTableRowSpan("Last discharged energy:", "no data", "lastDischarge").c_str();
    _content += _fp.getHtmlTableRowSpan("Deepest discharged energy:", "no data", "deepestDischarge").c_str();

    _content += _fp.getHtmlTableRowSpan("min Temperatur: ", "no data", "deepestTemperatur").c_str();
    _content += _fp.getHtmlTableRowSpan("max Temperatur: ", "no data", "highestTemperatur").c_str();

    _content += _fp.getHtmlTableRowSpan("Charge cycles: ", "no data", "numChargeCycles").c_str();
    _content += _fp.getHtmlTableRowSpan("Auto syncs: ", "no data", "numAutoSyncs").c_str();
    _content += _fp.getHtmlTableRowSpan("Time since last full:", "no data", "TimeSinceLastFull").c_str();



    _content += _fp.getHtmlTableEnd().c_str();
    _content += _fp.getHtmlFieldsetEnd().c_str();

    _content += _fp.addNewLine(2).c_str();

    _content += "<form action = '/resetstats' method = 'get'><button type = 'submit'>Reset statistics</button></form></br>";

    _content += _fp.getHtmlTable().c_str();
    _content += _fp.getHtmlTableRowText("<a href='/'>Main page</a>").c_str();
    _content += _fp.getHtmlTableRowText("<a href = 'config'>Configuration</a>").c_str();
    _content += _fp.getHtmlTableRowText("<a href = 'webserial'>Sensor monitoring</a>").c_str();

    _content += _fp.getHtmlTableEnd().c_str();

    _content += _fp.getHtmlTableColEnd().c_str();
    _content += _fp.getHtmlTableRowEnd().c_str();
    _content += _fp.getHtmlTableEnd().c_str();
    _content += _fp.getHtmlEnd().c_str();

    AsyncWebServerResponse* _response = request->beginChunkedResponse("text/html", [_content](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {

        std::string _chunk = "";
        size_t _len = min(_content.length() - index, maxLen);
        if (_len > 0) {
            _chunk = _content.substr(index, _len);
            _chunk.copy((char*)buffer, _chunk.length());
        }
        if (index + _len <= _content.length())
            return _chunk.length();
        else
            return 0;

        });
    _response->setContentLength(_content.length());
    _response->addHeader("Server", "ESP Async Web Server");
    request->send(_response);
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

