/*
  Estação meteorológica
  ESP32-S2 Mini + tela redonda 2,1" 360x360 (driver GC9B72, SPI)

  Bibliotecas (Gerenciador de Bibliotecas da IDE Arduino):
    - LovyanGFX   (versão 1.2.28 ou mais nova - é a que conhece o GC9B72)
    - ArduinoJson (versão 7)

  Placa: "LOLIN S2 Mini"   |   Partition Scheme: "No OTA (2MB APP/2MB SPIFFS)"

  Wi-Fi, cidade e pinos ficam no arquivo config.h (aba ao lado).

  Dados: Open-Meteo (https://open-meteo.com), gratuito e sem cadastro.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <driver/gpio.h>
#include "tela.h"
#include "config.h"   // Wi-Fi, cidade, pinos e demais ajustes
#include "sensor.h"

// Valores usados quando o seu config.h for de uma versão anterior e não tiver estes ajustes.
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 33
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 35
#endif
#ifndef AJUSTE_TEMPERATURA_CASA
#define AJUSTE_TEMPERATURA_CASA 0.0f
#endif
#ifndef MINUTOS_ENTRE_LEITURAS_SENSOR
#define MINUTOS_ENTRE_LEITURAS_SENSOR 30
#endif
#ifndef HORA_BRILHO_70
#define HORA_BRILHO_70 18      // a partir desta hora: 70% do brilho do dia
#endif
#ifndef HORA_BRILHO_40
#define HORA_BRILHO_40 20      // a partir desta hora: 40% do brilho do dia (até HORA_INICIO_NOITE)
#endif
#ifndef MHZ_PROCESSADOR
#define MHZ_PROCESSADOR 80     // 80 MHz basta para Wi-Fi e tela e gasta menos que 240
#endif


class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9B72 _painel;
  lgfx::Bus_SPI      _spi;
  lgfx::Light_PWM    _luz;

 public:
  LGFX() {
    {
      auto c = _spi.config();
      c.spi_host   = SPI2_HOST;
      c.spi_mode   = 0;
      c.freq_write = VELOCIDADE_SPI;
      c.pin_sclk   = PIN_SCL;
      c.pin_mosi   = PIN_SDA;
      c.pin_miso   = -1;
      c.pin_dc     = PIN_DC;
      c.dma_channel = 0;         // envio simples, sem DMA: um pouco mais lento, porém mais previsível no S2
      _spi.config(c);
      _painel.setBus(&_spi);
    }
    {
      auto c = _painel.config();
      c.pin_cs     = PIN_CS;
      c.pin_rst    = PIN_RST;
      c.pin_busy   = -1;
      c.readable   = false;
      c.invert     = INVERTER_CORES;
      c.rgb_order  = ORDEM_RGB;
      c.bus_shared = false;
      _painel.config(c);
    }
    {
      auto c = _luz.config();
      c.pin_bl      = PIN_BL;
      c.invert      = false;
      c.freq        = 12000;
      c.pwm_channel = 7;
      _luz.config(c);
      _painel.setLight(&_luz);
    }
    setPanel(&_painel);
  }
};

enum Resultado { DEU_CERTO, SEM_WIFI, SEM_CIDADE, SEM_DADOS };

LGFX  lcd;
Clima clima;

float    lat = LATITUDE, lon = LONGITUDE;
bool     temLocal = false;
bool     horaCerta = false;
int      bateria = -1;               // 0 a 100, -1 = não mostrar
int      ultimoMinuto = -1;
uint32_t proximaAtualizacao = 0;     // em millis()
const char* motivoReinicio = "";     // preenchido no setup quando o ESP reiniciou sozinho
int      ultimoErro = 0;             // código da última falha ao baixar dados
int      sinalWiFi = 0;              // força do sinal na última conexão, em dBm
bool     preferirHttp = false;       // vira true se o HTTPS falhar e o HTTP simples funcionar
SensoresCasa sensores;
bool     temSensor = false;
uint32_t ultimaLeituraSensor = 0;    // em millis()

// Mostra na tela em que passo a estação está (só enquanto ainda não há dados para exibir).
void etapa(const char* titulo, const char* linha) {
  Serial.printf("Etapa: %s\n", titulo);
  if (!clima.valido) tela::mensagem(titulo, linha, motivoReinicio);
}

// ----------------------- bateria -----------------------

float lerTensaoBateria() {
  uint32_t soma = 0;
  for (int i = 0; i < 16; ++i) {
    soma += analogReadMilliVolts(PIN_BATERIA);
    delay(2);
  }
  return soma / 16.0f * DIVISOR_BATERIA / 1000.0f;
}

// Curva aproximada de uma célula de lítio, com 0% na tensão de desligamento.
int porcentagemBateria(float v) {
  static const float V[] = { 3.50f, 3.62f, 3.70f, 3.75f, 3.79f, 3.83f, 3.88f, 3.94f, 4.00f, 4.08f, 4.17f };
  if (v <= V[0]) return 0;
  for (int i = 1; i <= 10; ++i) {
    if (v < V[i]) return (i - 1) * 10 + (int)((v - V[i - 1]) * 10.0f / (V[i] - V[i - 1]));
  }
  return 100;
}

// Apaga tudo e dorme. Acorda a cada 10 minutos só para medir a bateria de novo.
void dormirPorBateriaFraca(bool telaLigada) {
  Serial.println("Bateria fraca: desligando.");
  if (telaLigada) {
    tela::mensagem("Bateria fraca", "Conecte o carregador.", "A estação vai desligar.");
    delay(5000);
    lcd.setBrightness(0);
    lcd.sleep();
    delay(50);
  }
  gpio_hold_en((gpio_num_t)PIN_BL);   // mantém a luz de fundo apagada durante o sono
  gpio_deep_sleep_hold_en();
  esp_sleep_enable_timer_wakeup(10ULL * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void verificarBateria() {
  static int leiturasBaixas = 0;
  float v = lerTensaoBateria();
  if (v < 2.5f) {            // nada ligado no pino (ou chave da bateria desligada)
    bateria = -1;
    leiturasBaixas = 0;
    return;
  }
  bateria = porcentagemBateria(v);
  leiturasBaixas = (v < BATERIA_MINIMA) ? leiturasBaixas + 1 : 0;
  if (leiturasBaixas >= 3) dormirPorBateriaFraca(true);
}

// ----------------------- sensor da casa -----------------------

void lerSensores() {
  if (!temSensor) return;
  ultimaLeituraSensor = millis();
  float t, u, p;
  sensores.ler(t, u, p);
  clima.casaTemp    = isnan(t) ? NAN : t + AJUSTE_TEMPERATURA_CASA;
  clima.casaUmid    = u;
  clima.casaPressao = pressaoNivelDoMar(p, clima.elevacao);   // fica NAN até saber a altitude
  Serial.printf("Sensor: %.1f C, %.0f %%, %.1f hPa (local)\n", t, u, p);
}

// ----------------------- rede -----------------------

bool conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

void desligarWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// Faz um GET e devolve o corpo da resposta. Guarda o código do erro quando falha.
bool baixarCom(WiFiClient& cliente, const String& url, String& corpo) {
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  if (!http.begin(cliente, url)) {
    ultimoErro = -100;
    return false;
  }
  int codigo = http.GET();
  bool ok = (codigo == HTTP_CODE_OK);
  if (ok) {
    corpo = http.getString();
  } else {
    ultimoErro = codigo;
    Serial.printf("HTTP %d em %s\n", codigo, url.c_str());
  }
  http.end();
  return ok && corpo.length() > 0;
}

// Baixa "endereco" (sem o http:// na frente). Tenta primeiro a conexão segura (HTTPS);
// se ela falhar, tenta HTTP simples e passa a preferir o que funcionou.
// São só dados públicos de previsão, então o certificado não é validado.
bool baixar(const String& endereco, String& corpo) {
  for (int tentativa = 0; tentativa < 2; ++tentativa) {
    bool simples = (tentativa == 0) ? preferirHttp : !preferirHttp;
    bool ok;
    if (simples) {
      WiFiClient cliente;
      ok = baixarCom(cliente, "http://" + endereco, corpo);
    } else {
      WiFiClientSecure cliente;
      cliente.setInsecure();
      cliente.setHandshakeTimeout(10);   // segundos
      ok = baixarCom(cliente, "https://" + endereco, corpo);
    }
    if (ok) {
      preferirHttp = simples;
      return true;
    }
  }
  return false;
}

String codificarUrl(const char* s) {
  static const char HEXA[] = "0123456789ABCDEF";
  String r;
  for (; *s; ++s) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.') {
      r += (char)c;
    } else {
      r += '%';
      r += HEXA[c >> 4];
      r += HEXA[c & 15];
    }
  }
  return r;
}

// Devolve DEU_CERTO, SEM_DADOS (o serviço não respondeu) ou SEM_CIDADE (respondeu, mas não achou).
int acharCidade() {
  String url = "geocoding-api.open-meteo.com/v1/search?count=1&language=pt&format=json&name=";
  url += codificarUrl(CIDADE);
  if (PAIS[0]) {
    url += "&countryCode=";
    url += PAIS;
  }
  String corpo;
  if (!baixar(url, corpo)) return SEM_DADOS;
  if (!lerCidade(corpo.c_str(), clima.cidade, sizeof(clima.cidade), lat, lon)) return SEM_CIDADE;
  Serial.printf("Cidade: %s (%.4f, %.4f)\n", clima.cidade, lat, lon);
  return DEU_CERTO;
}

void acertarRelogio() {
  sntp_get_sync_status();   // descarta o resultado de uma sincronização anterior
  configTime(clima.utcOffset, 0, "pool.ntp.org", "time.google.com", "a.st1.ntp.br");
  uint32_t inicio = millis();
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && millis() - inicio < 8000) delay(100);
  struct tm t;
  horaCerta = getLocalTime(&t, 10);
}

int atualizarDados() {
  etapa("Conectando ao Wi-Fi", WIFI_NOME);
  if (!conectarWiFi()) {
    desligarWiFi();
    return SEM_WIFI;
  }
  sinalWiFi = WiFi.RSSI();
  ultimoErro = 0;

  int r = DEU_CERTO;
  if (!temLocal) {
    if (LATITUDE != 0 || LONGITUDE != 0) {
      snprintf(clima.cidade, sizeof(clima.cidade), "%s", CIDADE);
      temLocal = true;
    } else {
      etapa("Procurando a cidade", CIDADE);
      r = acharCidade();
      temLocal = (r == DEU_CERTO);
    }
  }

  if (temLocal) {
    etapa("Buscando a previsão", clima.cidade);
    String local = "latitude=" + String(lat, 4) + "&longitude=" + String(lon, 4);
    String corpo;
    String url = "api.open-meteo.com/v1/forecast?" + local +
                 "&current=temperature_2m,relative_humidity_2m,is_day,weather_code,pressure_msl,"
                 "wind_speed_10m,wind_direction_10m,uv_index"
                 "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1";
    if (baixar(url, corpo) && lerPrevisao(corpo.c_str(), clima)) {
      url = "air-quality-api.open-meteo.com/v1/air-quality?" + local + "&current=us_aqi&timezone=auto";
      if (!baixar(url, corpo) || !lerAr(corpo.c_str(), clima)) clima.aqi = -1;
      if (!horaCerta) tela::mensagem("Acertando o relógio", clima.cidade, motivoReinicio);
      acertarRelogio();
    } else {
      r = SEM_DADOS;
    }
  }

  desligarWiFi();
  return r;
}

// ----------------------- tela -----------------------

// Brilho conforme a hora: dia cheio, 70% a partir das 18h, 40% a partir das 20h e o brilho de noite depois das 22h.
uint8_t brilhoPara(int hora) {
  bool noite = (HORA_INICIO_NOITE > HORA_FIM_NOITE) ? (hora >= HORA_INICIO_NOITE || hora < HORA_FIM_NOITE)
                                                     : (hora >= HORA_INICIO_NOITE && hora < HORA_FIM_NOITE);
  if (noite) return BRILHO_NOITE;
  if (hora >= HORA_BRILHO_40) return (uint8_t)(BRILHO_DIA * 40 / 100);
  if (hora >= HORA_BRILHO_70) return (uint8_t)(BRILHO_DIA * 70 / 100);
  return BRILHO_DIA;
}

void redesenhar() {
  struct tm t;
  bool temHora = horaCerta && getLocalTime(&t, 0);
  lcd.setBrightness(temHora ? brilhoPara(t.tm_hour) : BRILHO_DIA);
  ultimoMinuto = temHora ? t.tm_min : -1;
  tela::principal(clima, temHora ? &t : nullptr, bateria);
}

void atualizar() {
  int r = atualizarDados();
  clima.semSinal = (r != DEU_CERTO);
  if (r == DEU_CERTO) {
    proximaAtualizacao = millis() + (uint32_t)MINUTOS_ENTRE_ATUALIZACOES * 60000UL;
  } else {
    proximaAtualizacao = millis() + (clima.valido ? 300000UL : 60000UL);   // tenta de novo em 5 min (1 min se ainda não há dados)
    Serial.printf("Falha na atualizacao (%d)\n", r);
  }

  if (clima.valido) {
    if (ultimaLeituraSensor == 0 || millis() - ultimaLeituraSensor > 60000UL) lerSensores();
    Serial.println("Etapa: desenhando a tela");
    redesenhar();
  } else if (r == SEM_WIFI) {
    tela::mensagem("Sem Wi-Fi", "Não consegui conectar em", WIFI_NOME);
  } else if (r == SEM_CIDADE) {
    tela::mensagem("Cidade não encontrada", CIDADE, "Confira o nome no código.");
  } else {
    char detalhe[48];
    snprintf(detalhe, sizeof(detalhe), "erro %d · sinal Wi-Fi %d dBm", ultimoErro, sinalWiFi);
    tela::mensagem("Sem resposta", "do serviço de previsão", detalhe);
  }
}

// ----------------------- início -----------------------

void setup() {
  setCpuFrequencyMhz(MHZ_PROCESSADOR);
  Serial.begin(115200);
  pinMode(PIN_BL, OUTPUT);      // luz de fundo apagada até a tela estar pronta
  digitalWrite(PIN_BL, LOW);

  // Antes de acender qualquer coisa: se a bateria ainda está fraca, volta a dormir.
  if (MEDIR_BATERIA) {
    float v = lerTensaoBateria();
    bool acordouDoSono = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
    float limite = acordouDoSono ? BATERIA_MINIMA + 0.20f : BATERIA_MINIMA;
    if (v >= 2.5f && v < limite) dormirPorBateriaFraca(false);
  }
  gpio_hold_dis((gpio_num_t)PIN_BL);
  gpio_deep_sleep_hold_dis();

  lcd.init();
  lcd.setRotation(ROTACAO);
  lcd.setBrightness(BRILHO_DIA);

  // Aviso desenhado direto na tela: se ele aparecer, tela e fios estão funcionando.
  lcd.fillScreen(0x0B1A33u);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(0xFFFFFFu);
  lcd.setTextDatum(lgfx::textdatum_t::middle_center);
  lcd.drawString("Iniciando...", 180, 180);

  if (!tela::iniciar(&lcd)) {
    Serial.println("Faltou memoria para a tela.");
    lcd.drawString("Sem memoria", 180, 220);
  }

  switch (esp_reset_reason()) {
    case ESP_RST_BROWNOUT: motivoReinicio = "(reiniciou: queda de tensão)"; break;
    case ESP_RST_PANIC:    motivoReinicio = "(reiniciou: erro no programa)"; break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      motivoReinicio = "(reiniciou: travamento)"; break;
    default: break;
  }
  if (motivoReinicio[0]) Serial.println(motivoReinicio);

  temSensor = sensores.iniciar(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("Sensor AHT20: %s | BMP280: %s\n", sensores.temAHT() ? "ok" : "nao encontrado",
                sensores.temBMP() ? "ok" : "nao encontrado");

  if (MEDIR_BATERIA) verificarBateria();
  atualizar();
}

void loop() {
  static uint32_t ultimaBateria = 0;
  uint32_t agora = millis();

  if (MEDIR_BATERIA && agora - ultimaBateria >= 30000) {
    ultimaBateria = agora;
    verificarBateria();
  }

  if (temSensor && agora - ultimaLeituraSensor >= (uint32_t)MINUTOS_ENTRE_LEITURAS_SENSOR * 60000UL) lerSensores();

  if ((int32_t)(agora - proximaAtualizacao) >= 0) atualizar();

  if (clima.valido) {
    struct tm t;
    int minuto = (horaCerta && getLocalTime(&t, 0)) ? t.tm_min : -1;
    if (minuto != ultimoMinuto) redesenhar();
  }
  delay(200);
}

// O HTTPS e o desenho da tela precisam de mais pilha que o padrão de 8 kB.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);
