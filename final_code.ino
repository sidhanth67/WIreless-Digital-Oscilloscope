#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <math.h>

const char* ssid = "OscilloscopeTest";
const char* password = "123456789";

AsyncWebServer server(80);
AsyncWebSocket webSocket("/ws");

float sharedData[10];
char msgBuffer[256];

TaskHandle_t SamplingTask;


const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
  <head>
    <title>ESP32 Precision Scope</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body {
        background: #121212;
        color: #e0e0e0;
        text-align: center;
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        margin: 0;
        display: flex;
        flex-direction: column;
        align-items: center;
      }
      h2 { margin-top: 10px; color: #03dac6; }
      .scope-container {
        position: relative;
        margin: 10px;
        border: 2px solid #333;
        box-shadow: 0 0 10px rgba(0, 255, 0, 0.1);
      }
      canvas {
        background: #000;
        display: block;
      }
      #voltageFollower {
        position: absolute;
        background: rgba(0, 0, 0, 0.8);
        color: #03dac6;
        padding: 2px 5px;
        border: 1px solid #03dac6;
        border-radius: 4px;
        font-size: 12px;
        pointer-events: none;
        display: none;
      }
      .stats-grid {
        display: grid;
        grid-template-columns: repeat(4, 1fr);
        gap: 15px;
        width: 90%;
        max-width: 600px;
        margin: 10px 0;
        background: #1e1e1e;
        padding: 15px;
        border-radius: 8px;
        border: 1px solid #333;
      }
      .stat-item {
        display: flex;
        flex-direction: column;
        align-items: center;
      }
      .stat-label {
        font-size: 0.8em;
        color: #bbb;
        text-transform: uppercase;
        letter-spacing: 1px;
      }
      .stat-value {
        font-size: 1.2em;
        font-weight: bold;
        color: #cf6679;
        font-family: monospace;
      }
      #disp_type { color: #bb86fc; }
      .controls {
        display: flex;
        flex-wrap: wrap;
        gap: 15px;
        justify-content: center;
        margin-bottom: 20px;
        background: #1e1e1e;
        padding: 10px;
        border-radius: 8px;
      }
      .control-group {
        display: flex;
        flex-direction: column;
        font-size: 0.9em;
      }
      input[type=range] { cursor: pointer; }
    </style>
  </head>
  <body>
    <h2>ESP32 Precision Scope</h2>

    <div class="scope-container">
      <canvas id="graph" width="360" height="250"></canvas>
      <div id="voltageFollower">0.00V</div>
    </div>

    <div class="stats-grid">
      <div class="stat-item">
        <span class="stat-label">Vrms</span>
        <span class="stat-value" id="disp_rms">0.00 V</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Vavg</span>
        <span class="stat-value" id="disp_avg">0.00 V</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Vpp</span>
        <span class="stat-value" id="disp_pp">0.00 V</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Wave</span>
        <span class="stat-value" id="disp_type">---</span>
      </div>
    </div>

    <div class="controls">
      <div class="control-group">
        <label>Timebase (X)</label>
        <input type="range" min="2" max="100" id="scaleX" value="50" />
      </div>
      <div class="control-group">
        <label>Gain (Y)</label>
        <input type="range" min="1" max="100" id="scaleY" value="50" />
      </div>
      <div class="control-group">
        <label>Offset (Y)</label>
        <input type="range" min="1" max="11" id="offsetY" value="6" />
      </div>
    </div>

    <script>
      const canvas = document.getElementById("graph");
      const ctx = canvas.getContext("2d");
      const elRMS = document.getElementById("disp_rms");
      const elAVG = document.getElementById("disp_avg");
      const elPP = document.getElementById("disp_pp");
      const elType = document.getElementById("disp_type");
      const tooltip = document.getElementById("voltageFollower");

      let dataBuffer = [];
      const MAX_POINTS = 1000;
      let incomingQ = [];

      let gateway = `ws://${window.location.hostname}/ws`;
      let socket;

      function initWebSocket() {
        socket = new WebSocket(gateway);
        socket.onopen = () => console.log("WS Connected");
        socket.onclose = () => setTimeout(initWebSocket, 2000);
        socket.onmessage = (e) => {
          let parts = e.data.split("|");
          if(parts[0]) {
            let vals = parts[0].split(",").map(Number);
            incomingQ.push(...vals);
            if(incomingQ.length > 200) incomingQ = incomingQ.slice(-200);
          }
          if(parts[1]) {
            let s = parts[1].split(",");
            if(s.length >= 4) {
              elRMS.innerText = parseFloat(s[0]).toFixed(2) + " V";
              elAVG.innerText = parseFloat(s[1]).toFixed(2) + " V";
              elPP.innerText  = parseFloat(s[2]).toFixed(2) + " V";
              elType.innerText = s[3];
            }
          }
        };
      }

      function draw() {
        ctx.fillStyle = "#000";
        ctx.fillRect(0,0, canvas.width, canvas.height);

        let scaleX = document.getElementById("scaleX").value * 10;
        let scaleY = document.getElementById("scaleY").value * 10;
        let offY = document.getElementById("offsetY").value;
        let offsetY = offY - 0.5 - 11/2;

        let yMin = parseInt(document.getElementById("scaleY").min);
        let yMax = parseInt(document.getElementById("scaleY").max);
        let yDiv = ((scaleY - yMin*10)/10 / (yMax - yMin)) * 6.5 + 0.1;

        let midY = canvas.height / 2 - (offsetY * 0.5 * canvas.height) / yDiv;
        ctx.beginPath();
        ctx.strokeStyle = "#444";
        ctx.moveTo(0, midY);
        ctx.lineTo(canvas.width, midY);
        ctx.stroke();

        ctx.beginPath();
        ctx.strokeStyle = "#03dac6";
        ctx.lineWidth = 2;

        let startIdx = dataBuffer.length < scaleX ? 0 : dataBuffer.length - scaleX;

        for(let i=startIdx; i<dataBuffer.length; i++) {
          let x = (i - startIdx) * (canvas.width / scaleX);
          let val = dataBuffer[i];
          let y = canvas.height - (val + offsetY + yDiv/2) * (canvas.height/yDiv);
          if(i === startIdx) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }

      function loop() {
        if(incomingQ.length > 0) {
          let batch = incomingQ.splice(0, 5);
          dataBuffer.push(...batch);
          if(dataBuffer.length > MAX_POINTS)
            dataBuffer = dataBuffer.slice(-MAX_POINTS);
        }
        draw();
        requestAnimationFrame(loop);
      }

      canvas.addEventListener("mousemove", (e) => {
        let rect = canvas.getBoundingClientRect();
        let y = e.clientY - rect.top;
        let scaleY = document.getElementById("scaleY").value * 10;
        let yMin = parseInt(document.getElementById("scaleY").min);
        let yMax = parseInt(document.getElementById("scaleY").max);
        let yDiv = ((scaleY - yMin*10)/10 / (yMax - yMin)) * 6.5 + 0.1;
        let offY = document.getElementById("offsetY").value;
        let offsetY = offY - 0.5 - 11/2;
        let volts = (yDiv/2 - (y/canvas.height)*yDiv - offsetY);

        tooltip.style.left = (e.clientX + 15) + "px";
        tooltip.style.top = (e.clientY - 20) + "px";
        tooltip.style.display = "block";
        tooltip.innerText = volts.toFixed(2) + "V";
      });

      canvas.addEventListener("mouseleave", () => {
        tooltip.style.display = "none";
      });

      window.onload = () => {
        initWebSocket();
        loop();
      };
    </script>
  </body>
</html>
)rawliteral";

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void * arg, uint8_t *data, size_t len) {}

String identifyWave(float vRMS, float vPP) {
  if (vPP < 0.2) return "Noise";
  float peak = vPP / 2.0;
  if (peak == 0) return "--";
  float ratio = vRMS / peak;
  if (ratio > 0.90) return "Square";
  if (ratio > 0.65) return "Sine";
  if (ratio > 0.50) return "Triangle";
  return "Complex";
}

void coreSamplingCode(void * pvParameters) {
  int batchIdx = 0;

  #define BUF_SIZE 500
  float analysisBuf[BUF_SIZE];
  int aIdx = 0;
  int state = 0;
  int zeroCrossings = 0;
  int startIndex = 0;
  float prevVal = 0;

  float outRMS = 0, outAVG = 0, outPP = 0;
  String outType = "Wait..";

  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(5);
  xLastWakeTime = xTaskGetTickCount();

  for(;;) {
    float val = (3.3 * analogRead(34) / 4095.0) * 2 - 3.3;

    sharedData[batchIdx++] = val;

    if(aIdx < BUF_SIZE) analysisBuf[aIdx] = val;

    bool crossingUp = (prevVal < 0 && val >= 0);

    if (state == 0 && crossingUp) {
      state = 1;
      startIndex = aIdx;
      zeroCrossings = 0;
    }
    else if (state == 1 && crossingUp) {
      zeroCrossings++;
      if (zeroCrossings >= 2) {
        int endIndex = aIdx;
        float sum=0, sumSq=0;
        float maxV=-100, minV=100;
        int count=0;

        for(int k=startIndex; k<=endIndex; k++){
          float v = analysisBuf[k];
          sum += v;
          sumSq += (v*v);
          if(v > maxV) maxV = v;
          if(v < minV) minV = v;
          count++;
        }

        if(count > 0) {
          outAVG = sum / count;
          outRMS = sqrt(sumSq / count);
          outPP = maxV - minV;
          outType = identifyWave(outRMS, outPP);
        }

        state = 0;
        aIdx = -1;
      }
    }

    if (aIdx >= BUF_SIZE - 1) {
      state = 0;
      aIdx = -1;
      outType = "No Sync";
    }

    aIdx++;
    prevVal = val;

    if (batchIdx >= 10) {
      batchIdx = 0;
      if (webSocket.count() > 0 && webSocket.availableForWriteAll()) {
        snprintf(msgBuffer, sizeof(msgBuffer),
          "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f|%.2f,%.2f,%.2f,%s",
          sharedData[0], sharedData[1], sharedData[2], sharedData[3], sharedData[4],
          sharedData[5], sharedData[6], sharedData[7], sharedData[8], sharedData[9],
          outRMS, outAVG, outPP, outType.c_str()
        );
        webSocket.textAll(msgBuffer);
      }
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  webSocket.onEvent(onWebSocketEvent);
  server.addHandler(&webSocket);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(200, "text/html", index_html);
  });
  server.begin();

  xTaskCreatePinnedToCore(
    coreSamplingCode,
    "SamplingTask",
    10000,
    NULL,
    1,
    &SamplingTask,
    1
  );
}

void loop() {
  webSocket.cleanupClients();
  vTaskDelay(pdMS_TO_TICKS(2000));
}
