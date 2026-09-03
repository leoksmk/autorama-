# Autorama — Controle de motor via Wi-Fi (ESP32-C3 SuperMini)

Firmware para controlar o motor da PCB pelo celular, via navegador.
A placa cria sua própria rede Wi-Fi; você conecta o celular e abre um IP.

## O que este hardware faz (e o que não faz)

O esquemático tem **um motor** acionado por um **Darlington NPN (Q1) como chave
low-side** no GPIO4. Isso significa:

- ✅ **Só vai pra frente**, com velocidade por PWM.
- ❌ **Sem marcha ré** e **sem direção** — não há ponte H. É um *acelerador*.

### Como é o controle no celular

Um **botão único**. Cada toque dá um "empurrão" na velocidade; se você parar de
tocar, a velocidade **cai sozinha** até o carrinho parar. Ou seja, tem que ficar
apertando o botão pra andar e pra manter a velocidade (isso também serve de
failsafe: perdeu o sinal, o carrinho para).

### Atenção no hardware (18650)

- **Brownout com a bateria caindo.** A 18650 vai de ~4,2 V (cheia) a ~3,0 V
  (vazia) e alimenta o pino "5V" da SuperMini. O regulador precisa de ~3,5 V na
  entrada; abaixo disso o ESP reseta sozinho — antes da célula esvaziar de fato.
- **Pico de corrente do motor.** Ao arrancar/travar, o motor derruba a tensão por
  um instante e pode resetar o ESP. O C1 ajuda; motor grande pode não bastar.

## Como usar

1. Instale o **Arduino core para ESP32 (versão 3.x)** — este código usa a API
   `ledcAttach`/`ledcWrite` nova.
2. Placa: **ESP32C3 Dev Module** (ou ESP32-C3 SuperMini). Abra `autorama/autorama.ino`.
3. Faça o upload.
4. No celular, conecte na rede Wi-Fi **`Autorama`** (senha `12345678`).
5. Abra o navegador em **http://192.168.4.1**
6. **Aperte o botão ACELERAR várias vezes** pra andar. Parou de apertar, para.

### Usar a sua rede de casa (opcional)

No topo do `.ino`, mude `#define USE_STA 0` para `1` e preencha `STA_SSID`/`STA_PASS`.
O IP aparece no Serial Monitor (115200 baud).

## Ajustes rápidos (no `.ino`)

| Constante          | Para quê                                                        |
|--------------------|-----------------------------------------------------------------|
| `AP_SSID/PASS`     | Nome e senha da rede criada pela placa                          |
| `MOTOR_PIN`        | Pino do motor (padrão GPIO4)                                    |
| `TAP_STEP`         | Quanto cada toque acelera (maior = pega velocidade mais rápido) |
| `DECAY_PER_100MS`  | Quão rápido a velocidade cai sem tocar (maior = para mais cedo) |
| `MOTOR_MIN`        | Velocidade mínima em que o motor gira (abaixo disso corta p/ 0) |
| `PWM_FREQ`         | Frequência do PWM (1 kHz padrão)                               |
