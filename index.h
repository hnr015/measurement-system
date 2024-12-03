#ifndef INDEX_H
#define INDEX_H

const char* mainPage = R"HTML(
<!DOCTYPE html>
<html>
<head>
<title>System pomiarowy</title>
  <style>
  body {
    font-family: sans-serif;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
    background-color: #f0f0f0;
    margin: 0; 
  }
  .container {
    font-size: 32px;
    background-color: white;
    border-radius: 10px;
    padding: 20px;
    box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
    text-align: center;
  }
  .measurement {
    margin-bottom: 15px;
  }
  .value {
    font-size: 36px;
    font-weight: bold;
  }
  .unit {
    font-size: 32px;
  }
  .warning {
    color: red;
    font-weight: bold;
  }
  </style>
</head>
<body>
  <div class="container">
    <h2>System pomiarowy</h2>
    <div class="link">
      <a href="http://192.168.1.125/system_pomiarowydb/charts.html" style="position: absolute; top: 20px; left: 20px; text-decoration: none; color: #3498db; font-weight: bold; font-size: 16px;">Wykresy</a>
    </div>
    <div class="measurement">
      <span class="label">Temperatura:</span>
      <span class="value" id="temperature">ładowanie...</span>
      <span class="unit" id="temperatureUnit"></span>
    </div>
    <div class="measurement">
      <span class="label">Wilgotność:</span>
      <span class="value" id="humidity">ładowanie...</span>
      <span class="unit" id="humidityUnit"></span>
    </div>
    <div class="measurement">
      <span class="label">LPG:</span>
      <span class="value" id="lpg">ładowanie...</span>
      <span class="unit" id="lpgUnit"></span> <span id="lpgWarning"></span>
    </div>
    <div class="measurement">
      <span class="label">CO:</span>
      <span class="value" id="co">ładowanie...</span>
      <span class="unit" id="coUnit"></span> <span id="coWarning"></span>
    </div>
    <div class="measurement">
      <span class="label">CO2:</span>
      <span class="value" id="smoke">ładowanie...</span>
      <span class="unit" id="smokeUnit"></span> <span id="smokeWarning"></span>
    </div>
  </div>
  <script>
    function updateData() {
      fetch('/data')
      .then(response => response.json())
      .then(data => {
        document.getElementById('temperature').textContent = data.t.toFixed(1);
        document.getElementById('temperatureUnit').textContent = " °C";
        document.getElementById('humidity').textContent = data.h.toFixed(1);
        document.getElementById('humidityUnit').textContent = " %";

        updateGasMeasurement(data.lpg, 'lpg', 'lpgUnit', 'lpgWarning');
        updateGasMeasurement(data.co, 'co', 'coUnit', 'coWarning');
        updateGasMeasurement(data.smoke, 'smoke', 'smokeUnit', 'smokeWarning');

      })
      .catch(error => {
        console.error('Błąd:', error);
        document.getElementById('data').innerHTML = `<p>Błąd: ${error.message}</p>`;
      });
    }


    function updateGasMeasurement(value, id, unitId, warningId) {
      const valueSpan = document.getElementById(id);
      const unitSpan = document.getElementById(unitId);
      const warningSpan = document.getElementById(warningId);

      if (typeof value === 'number') {
        if (value > 9999) {
          valueSpan.textContent = "poza skalą";
          unitSpan.textContent = "";
          warningSpan.textContent = "";
        } else {
          valueSpan.textContent = value;
          unitSpan.textContent = " ppm";

          if (value > 30) {
            warningSpan.textContent = " - Przekroczony poziom!";
            valueSpan.style.color = 'red';
            warningSpan.style.color = 'red';
          } else {
            warningSpan.textContent = "";
            valueSpan.style.color = 'black';
          }
        }
      } else {
        valueSpan.textContent = value; 
        unitSpan.textContent = "";
        warningSpan.textContent = "";
      }
    }
    setInterval(updateData, 5000);
    updateData();
  </script>
</body>
</html>
)HTML";

#endif