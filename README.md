# Autorama — Controle de motor via Wi-Fi (ESP32-C3 SuperMini)

Firmware para controlar o motor da PCB pelo celular, via navegador.
A placa cria sua própria rede Wi-Fi; você conecta o celular e abre um IP.

## O que este hardware faz (e o que não faz)

O esquemático tem **um motor** acionado por um **Darlington NPN (Q1) como chave
low-side** no GPIO4. Isso significa:

- ✅ Liga/desliga e **controle de velocidade por PWM** (uma direção).
- ❌ **Sem marcha ré** e **sem direção/esterçamento** — não há ponte H.
  É um *acelerador* (ideal para autorama / carrinho de pista), não um RC dirigível.

### Dois pontos de atenção no hardware

1. **Leitura de bateria (GPIO20 não tem ADC).** No ESP32-C3 apenas
   **GPIO0–GPIO4** têm ADC. O net `bat_voltage_divisor` está no GPIO20, que é
   pino de UART, sem ADC — `analogRead` ali não funciona. Religue o divisor em
   **GPIO2 ou GPIO3** e ajuste `BAT_PIN` no código. Até lá, a tensão exibida é inválida.
2. **Alimentar o pino "5V" com +3V8 é marginal.** O regulador da placa fica perto
   do dropout; se houver reset por brownout quando o motor puxa corrente, essa é a causa.

## Como usar

1. Instale o **Arduino core para ESP32 (versão 3.x)** — este código usa a API
   `ledcAttach`/`ledcWrite` nova.
2. Placa: **ESP32C3 Dev Module** (ou ESP32-C3 SuperMini). Abra `autorama/autorama.ino`.
3. Faça o upload.
4. No celular, conecte na rede Wi-Fi **`Autorama`** (senha `12345678`).
5. Abra o navegador em **http://192.168.4.1**
6. Use o controle deslizante / botões. **PARAR** zera o motor.

### Usar a sua rede de casa (opcional)

No topo do `.ino`, mude `#define USE_STA 0` para `1` e preencha `STA_SSID`/`STA_PASS`.
O IP aparece no Serial Monitor (115200 baud).

## Segurança (failsafe)

Se o navegador parar de enviar comandos por mais de 1,5 s (celular travou, saiu do
alcance), o firmware **corta o motor** automaticamente. A página envia um
"heartbeat" a cada 0,7 s para manter o controle ativo.

## Ajustes rápidos (no `.ino`)

| Constante      | Para quê                                             |
|----------------|------------------------------------------------------|
| `AP_SSID/PASS` | Nome e senha da rede criada pela placa               |
| `MOTOR_PIN`    | Pino do motor (padrão GPIO4)                         |
| `BAT_PIN`      | Pino ADC do divisor de bateria (religue p/ GPIO2/3)  |
| `PWM_FREQ`     | Frequência do PWM (1 kHz padrão)                     |
| `FAILSAFE_MS`  | Tempo sem comando até parar o motor                  |
