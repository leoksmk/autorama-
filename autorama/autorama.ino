/*
 * Autorama - Controle de motor via Wi-Fi (ESP32-C3 SuperMini)
 * ------------------------------------------------------------
 * Hardware (conforme o esquematico):
 *   GPIO4  -> R1(330R) -> base do Q1 (Darlington NPN, low-side switch)
 *             Q1 liga/desliga o motor. So ha UMA direcao (sem ponte H).
 *             Controle = PWM (velocidade). So vai pra frente.
 *   D1(1N4007) = diodo de roda livre (flyback) em paralelo com o motor.
 *   C1         = capacitor de desacoplamento no motor.
 *   Bateria: 1x 18650 (~3,0 a 4,2 V) no rail +3V8.
 *
 * Controle (celular): UM botao. Cada toque da um "empurrao" na velocidade;
 *   se voce parar de tocar, a velocidade CAI sozinha ate parar. Ou seja,
 *   tem que ficar apertando pra andar e pra manter a velocidade.
 *
 * Requer: Arduino core para ESP32 versao 3.x (API ledcAttach/ledcWrite).
 *
 * Uso:
 *   1. Placa "ESP32C3 Dev Module" (ou "ESP32-C3 SuperMini"). Faca upload.
 *   2. No celular, conecte na rede Wi-Fi "Autorama" (senha abaixo).
 *   3. Abra o navegador em  http://192.168.4.1  e martele o botao.
 */

#include <WiFi.h>
#include <WebServer.h>

// ---------- Configuracao Wi-Fi ----------
// Modo AP (padrao): a propria placa cria a rede. Nao precisa de roteador.
const char* AP_SSID = "Autorama";
const char* AP_PASS = "12345678";   // minimo 8 caracteres

// Para usar sua rede de casa em vez do modo AP, defina USE_STA como 1.
#define USE_STA 0
const char* STA_SSID = "SUA_REDE";
const char* STA_PASS = "SUA_SENHA";

// ---------- Pino do motor ----------
const int MOTOR_PIN = 4;    // GPIO4 -> base do Darlington via R1

// ---------- PWM ----------
const int PWM_FREQ = 1000;  // 1 kHz - adequado p/ Darlington + motor DC
const int PWM_RES  = 8;     // 8 bits -> duty 0..255

// ---------- Comportamento do "aperta pra andar" ----------
const int TAP_STEP        = 22;   // quanto cada toque acrescenta (%)
const int DECAY_PER_100MS = 14;   // quanto cai a cada 100 ms sem tocar (%)
const int MOTOR_MIN       = 25;   // abaixo disso o motor nem gira: corta p/ 0

WebServer server(80);
int throttle = 0;                 // 0..100 (%)
unsigned long lastDecay = 0;

void writeMotor() {
  int pct = (throttle < MOTOR_MIN) ? 0 : throttle;
  int duty = map(pct, 0, 100, 0, (1 << PWM_RES) - 1);
  ledcWrite(MOTOR_PIN, duty);
}

void tap() {
  throttle = min(100, throttle + TAP_STEP);
  writeMotor();
}

// ---------- Pagina web ----------
const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="pt-br"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Autorama</title>
<style>
  :root{color-scheme:dark}
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none}
  html,body{height:100%}
  body{margin:0;font-family:system-ui,sans-serif;background:#0f1115;color:#e8eaed;
       display:flex;flex-direction:column;align-items:center;justify-content:center;
       gap:26px;padding:24px;touch-action:manipulation}
  h1{font-size:1.3rem;margin:0;color:#9aa4b2;font-weight:600}
  .bar{width:min(86vw,420px);height:16px;border-radius:8px;background:#232733;overflow:hidden}
  .fill{height:100%;width:0;background:linear-gradient(90deg,#4f8cff,#22d39a);transition:width .08s linear}
  .val{font-size:2.6rem;font-weight:700;font-variant-numeric:tabular-nums}
  #go{width:min(86vw,420px);height:min(46vh,300px);border:0;border-radius:28px;
      font-size:2rem;font-weight:800;color:#fff;
      background:radial-gradient(circle at 50% 35%,#3f6fd8,#243257);
      box-shadow:0 10px 0 #16203a,0 14px 24px rgba(0,0,0,.4)}
  #go:active{transform:translateY(8px);box-shadow:0 2px 0 #16203a,0 6px 14px rgba(0,0,0,.4)}
  .hint{font-size:.9rem;color:#6b7280;margin-top:-8px}
</style></head><body>
  <h1>&#127950; Autorama</h1>
  <div class="val"><span id="pct">0</span>%</div>
  <div class="bar"><div id="fill" class="fill"></div></div>
  <button id="go">ACELERAR</button>
  <div class="hint">aperte varias vezes pra andar</div>
<script>
  const go=document.getElementById('go'),pct=document.getElementById('pct'),
        fill=document.getElementById('fill');
  let busy=false;
  function draw(v){pct.textContent=v;fill.style.width=v+'%';}
  function hit(url){
    if(busy)return;busy=true;
    fetch(url).then(r=>r.json()).then(j=>{draw(j.t);busy=false;})
              .catch(_=>{busy=false;});
  }
  // cada toque = um empurrao
  go.addEventListener('pointerdown',e=>{e.preventDefault();hit('/tap');});
  // atualiza a barra enquanto a velocidade cai sozinha
  setInterval(()=>hit('/state'),120);
</script></body></html>
)HTML";

void handleRoot()  { server.send_P(200, "text/html", PAGE); }

void sendState() {
  char buf[24];
  snprintf(buf, sizeof(buf), "{\"t\":%d}", (throttle < MOTOR_MIN) ? 0 : throttle);
  server.send(200, "application/json", buf);
}

void handleTap()   { tap();  sendState(); }
void handleState() {         sendState(); }

void setup() {
  Serial.begin(115200);

  ledcAttach(MOTOR_PIN, PWM_FREQ, PWM_RES);   // core 3.x
  ledcWrite(MOTOR_PIN, 0);

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
  server.on("/tap", handleTap);
  server.on("/state", handleState);
  server.begin();
  lastDecay = millis();
}

void loop() {
  server.handleClient();

  // Queda automatica: sem toques, a velocidade cai ate parar (failsafe natural).
  unsigned long now = millis();
  if (now - lastDecay >= 100) {
    int steps = (now - lastDecay) / 100;
    lastDecay += (unsigned long)steps * 100;
    if (throttle > 0) {
      throttle = max(0, throttle - DECAY_PER_100MS * steps);
      writeMotor();
    }
  }
}
