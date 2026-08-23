#pragma once

#ifndef _HTML_h
#define _HTML_h

#include <Arduino.h>

const char html_update_data_function[] PROGMEM = R"=====(
function updateData(jsonData) {
   document.getElementById('RSSIValue').innerHTML = jsonData.rssi + "dBm";
   document.getElementById('VoltageValue').innerHTML = jsonData.voltage + "V";
   document.getElementById('CurrentValue').innerHTML = jsonData.current + "A";
   document.getElementById('AverageCurrentValue').innerHTML = jsonData.avgCurrent + "A";
   document.getElementById('SocValue').innerHTML = jsonData.soc + "%";
   document.getElementById('tTgValue').innerHTML = jsonData.ttg;
   document.getElementById('isFullValue').innerHTML = jsonData.isFull;
   document.getElementById('TemperatureValue').innerHTML = parseFloat(jsonData.temperature).toFixed(1) + "&deg;C";
   document.getElementById('Mode').innerHTML = jsonData.mode;
   
   // Statistics Updates
   document.getElementById('minBatVoltage').innerHTML = jsonData.minBatVoltage + "V";
   document.getElementById('maxBatVoltage').innerHTML = jsonData.maxBatVoltage + "V";
   document.getElementById('amountDischargedEnergy').innerHTML = jsonData.amountDischargedEnergy + "kWh";
   document.getElementById('amountChargedEnergy').innerHTML = jsonData.amountChargedEnergy + "kWh";
   document.getElementById('deepestDischarge').innerHTML = jsonData.deepestDischarge + "Ah";
   document.getElementById('lastDischarge').innerHTML = jsonData.lastDischarge + "Ah";
   document.getElementById('deepestTemperatur').innerHTML = parseFloat(jsonData.deepestTemperatur).toFixed(1) + "&deg;C";
   document.getElementById('highestTemperatur').innerHTML = parseFloat(jsonData.highestTemperatur).toFixed(1) + "&deg;C";
   document.getElementById('TimeSinceLastFull').innerHTML = jsonData.TimeSinceLastFull;
   document.getElementById('numChargeCycles').innerHTML = jsonData.numChargeCycles;
   document.getElementById('numAutoSyncs').innerHTML = jsonData.numAutoSyncs;
   document.getElementById('sumApHDrawn').innerHTML = jsonData.sumApHDrawn + "Ah";
}
)=====";

const char html_soc_response[] PROGMEM = R"=====(
<!DOCTYPE HTML>
<html>
  <head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="2; url=/#battery">
    <script type="text/javascript">
      window.location.href = "/#battery"
    </script>
    <title>SOC Set</title>
  </head>
  <body>
    <h2>SOC has been updated</h2>
    <a href='/#battery'>Back to Battery</a>
  </body>
</html>
)=====";

const char html_stats_reset_response[] PROGMEM = R"=====(
<!DOCTYPE HTML>
<html>
  <head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="2; url=/#statistics">
    <script type="text/javascript">
      window.location.href = "/#statistics"
    </script>
    <title>Statistics Reset</title>
  </head>
  <body>
    <h2>Statistics have been reset</h2>
    <a href='/#statistics'>Back to Statistics</a>
  </body>
</html>
)=====";

const char html_form_end[] PROGMEM = R"=====(
<br>
<form action='/reboot' method='get'><button type='submit'>Reboot</button></form>
<br>
<form action='/' method='get' style='display:inline;'>
  <button type='submit'>Home</button>
  <br>
</form>
<br>
<form action='/post' method='post' style='display:inline;' onsubmit="event.preventDefault(); postReset();">
  <button type='submit' class='danger'>Reset to Factory Defaults</button>
</form>
<script>
function postReset() {
    if (!confirm('Are you sure? This will erase all settings!')) {
        return;
    }
    fetch('/post', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'reset=true'
    })
    .then(response => {
        if (response.ok) { 
            alert('Reset successful.');
            window.location.reload(true);
        } else {
            alert('Reset failed!');
        }
    })
    .catch(error => {
        console.error('Reset failed:', error);
        alert('Reset failed: ' + error.message);
    });
}
</script>
)=====";

#endif