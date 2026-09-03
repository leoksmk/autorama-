/*
 * Autorama - Controle de motor via Wi-Fi (ESP32-C3 SuperMini)
 * ------------------------------------------------------------
 * Hardware (conforme o esquematico):
 *   GPIO4  -> R1(330R) -> base do Q1 (Darlington NPN, low-side switch)
 *             Q1 liga/desliga o motor. So ha UMA direcao (sem ponte H).
 *             Controle = PWM (velocidade), nao ha marcha re.
 *   GPIO?  -> bat_voltage_divisor (leitura de tensao da bateria)
 *             ATENCAO: no esquema esta em GPIO20, que NAO tem ADC no
 *             ESP32-C3. Para a leitura funcionar, ligue esse divisor
 *             em um pino ADC (GPIO2 ou GPIO3). Ajuste BAT_PIN abaixo.
 *   D1(1N4007) = diodo de roda livre (flyback) em paralelo com o motor.
 *   C1         = capacitor de desacoplamento no motor.
 *
 * Requer: Arduino core para ESP32 versao 3.x (API ledcAttach/ledcWrite).
 *
 * Uso:
 *   1. Selecione a placa "ESP32C3 Dev Module" (ou "ESP32-C3 SuperMini").
 *   2. Faca upload.
 *   3. No celular, conecte na rede Wi-Fi "Autorama" (senha abaixo).
 *   4. Abra o navegador em  http://192.168.4.1
 */

#include <WiFi.h>
#include <WebServer.h>

// ---------- Configuracao Wi-Fi ----------
// Modo AP (padrao): a propria placa cria a rede. Nao precisa de roteador.
const char* AP_SSID = "Autorama";
const char* AP_PASS = "12345678";   // minimo 8 caracteres

// Para usar sua rede de casa em vez do modo AP, defina USE_STA como 1
// e preencha STA_SSID / STA_PASS. Ai o IP aparece no Serial Monitor.
#define USE_STA 0
const char* STA_SSID = "SUA_REDE";
const char* STA_PASS = "SUA_SENHA";

// ---------- Pinos ----------
const int MOTOR_PIN = 4;    // GPIO4 -> base do Darlington via R1
const int BAT_PIN   = 2;    // ADC: mude p/ o pino onde ligou o divisor
                            // (GPIO20 do esquema NAO tem ADC no C3!)

// ---------- Divisor de tensao ----------
// bat = Vbat * R3 / (R2 + R3).  R2 = 350, R3 = 388  ->  fator ~0.5257
const float R2 = 350.0;
const float R3 = 388.0;
const float VREF = 3.3;     // tensao de referencia do ADC (aprox)

// ---------- PWM ----------
const int PWM_FREQ = 1000;  // 1 kHz - adequado p/ Darlington + motor DC
const int PWM_RES  = 8;     // 8 bits -> duty 0..255

// ---------- Failsafe ----------
// Se nenhum comando chegar nesse tempo, corta o motor (seguranca).
const unsigned long FAILSAFE_MS = 1500;

WebServer server(80);
int   throttle = 0;               // 0..100 (%)
unsigned long lastCmd = 0;

void applyThrottle(int pct) {
  pct = constrain(pct, 0, 100);
  throttle = pct;
  int duty = map(pct, 0, 100, 0, (1 << PWM_RES) - 1);
  ledcWrite(MOTOR_PIN, duty);
  lastCmd = millis();
}

float readBatteryVolts() {
  // media de algumas amostras p/ reduzir ruido
  long acc = 0;
  for (int i = 0; i < 16; i++) acc += analogRead(BAT_PIN);
  float raw = acc / 16.0;
  float vpin = (raw / 4095.0) * VREF;         // tensao no pino
  return vpin * (R2 + R3) / R3;               // desfaz o divisor
}

// ---------- Pagina web ----------
const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="pt-br"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Autorama</title>
<style>
  :root{color-scheme:dark}
  *{box-sizing:border-box}
  body{margin:0;font-family:system-ui,sans-serif;background:#0f1115;color:#e8eaed;
       display:flex;flex-direction:column;align-items:center;gap:22px;padding:24px}
  h1{font-size:1.4rem;margin:.2rem 0}
  .val{font-size:3.4rem;font-weight:700;font-variant-numeric:tabular-nums}
  input[type=range]{-webkit-appearance:none;width:min(90vw,420px);height:46px;
       border-radius:23px;background:#232733;outline:none}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:52px;height:52px;
       border-radius:50%;background:#4f8cff;border:3px solid #cfe0ff}
  input[type=range]::-moz-range-thumb{width:52px;height:52px;border-radius:50%;
       background:#4f8cff;border:3px solid #cfe0ff}
  .row{display:flex;gap:12px;flex-wrap:wrap;justify-content:center}
  button{font-size:1rem;padding:14px 20px;border:0;border-radius:12px;color:#fff;
       background:#2a2f3a;min-width:90px}
  button:active{transform:scale(.96)}
  .stop{background:#e5484d;font-weight:700;flex:1;min-width:min(90vw,420px);padding:20px}
  .bat{font-size:.95rem;color:#9aa4b2}
</style></head><body>
  <h1>&#127950; Autorama</h1>
  <div class="val"><span id="pct">0</span>%</div>
  <input id="sl" type="range" min="0" max="100" value="0">
  <div class="row">
    <button onclick="preset(25)">25%</button>
    <button onclick="preset(50)">50%</button>
    <button onclick="preset(75)">75%</button>
    <button onclick="preset(100)">100%</button>
  </div>
  <button class="stop" onclick="preset(0)">PARAR</button>
  <div class="bat">Bateria: <span id="bat">--</span> V</div>
<script>
  const sl=document.getElementById('sl'),pct=document.getElementById('pct'),
        bat=document.getElementById('bat');
  let pend=0,busy=false;
  function send(v){pend=v;flush();}
  function flush(){
    if(busy)return;busy=true;
    fetch('/set?v='+pend).then(r=>r.json()).then(j=>{
      bat.textContent=j.bat.toFixed(2);busy=false;
    }).catch(_=>{busy=false;});
  }
  function ui(v){v=Math.round(v);sl.value=v;pct.textContent=v;}
  sl.addEventListener('input',()=>{ui(sl.value);send(sl.value);});
  function preset(v){ui(v);send(v);}
  // heartbeat: mantem o failsafe feliz e atualiza a bateria
  setInterval(()=>send(sl.value),700);
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", PAGE); }

void handleSet() {
  if (server.hasArg("v")) applyThrottle(server.arg("v").toInt());
  float v = readBatteryVolts();
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"t\":%d,\"bat\":%.2f}", throttle, v);
  server.send(200, "application/json", buf);
}

void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_PIN, OUTPUT);
  ledcAttach(MOTOR_PIN, PWM_FREQ, PWM_RES);   // core 3.x
  ledcWrite(MOTOR_PIN, 0);

  analogReadResolution(12);                   // 0..4095

#if USE_STA
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println();
  Serial.print("IP: http://"); Serial.println(WiFi.localIP());
#else
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP \""); Serial.print(AP_SSID);
  Serial.print("\"  ->  http://"); Serial.println(WiFi.softAPIP());
#endif

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
  lastCmd = millis();
}

void loop() {
  server.handleClient();

  // Failsafe: perdeu comunicacao -> para o motor
  if (throttle > 0 && millis() - lastCmd > FAILSAFE_MS) {
    applyThrottle(0);
    Serial.println("Failsafe: motor parado (sem comando).");
  }
}
