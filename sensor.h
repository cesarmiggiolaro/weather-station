// Sensor de ambiente AHT20 (temperatura e umidade) + BMP280 (pressão e temperatura),
// lidos direto pelo barramento I2C, sem bibliotecas extras.
// Se algum dos dois não responder, o outro continua funcionando.
#pragma once
#include <Wire.h>
#include <math.h>

class SensoresCasa {
 public:
  // Liga o I2C e procura os sensores. Devolve true se achou pelo menos um.
  bool iniciar(int sda, int scl) {
    Wire.begin(sda, scl, 100000);
    _aht = iniciarAHT();
    _bmp = iniciarBMP(0x77) || iniciarBMP(0x76);
    return _aht || _bmp;
  }

  bool temAHT() const { return _aht; }
  bool temBMP() const { return _bmp; }

  // Faz uma leitura. Valores que não puderam ser lidos ficam NAN.
  // temp em °C, umid em %, pressao em hPa (pressão local, sem corrigir para o nível do mar).
  void ler(float& temp, float& umid, float& pressao) {
    temp = umid = pressao = NAN;
    float tBmp = NAN;
    if (_bmp) lerBMP(tBmp, pressao);
    if (_aht) lerAHT(temp, umid);
    if (isnan(temp)) temp = tBmp;
  }

  // ---------- conversões do BMP280 (folha de dados da Bosch, seção 8.2) ----------
  struct CalibBMP {
    uint16_t T1; int16_t T2, T3;
    uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9;
  };

  // Devolve a temperatura em centésimos de grau e preenche tFine.
  static int32_t compensarTemp(const CalibBMP& c, int32_t adcT, int32_t& tFine) {
    int32_t v1 = ((((adcT >> 3) - ((int32_t)c.T1 << 1))) * ((int32_t)c.T2)) >> 11;
    int32_t v2 = (((((adcT >> 4) - ((int32_t)c.T1)) * ((adcT >> 4) - ((int32_t)c.T1))) >> 12) * ((int32_t)c.T3)) >> 14;
    tFine = v1 + v2;
    return (tFine * 5 + 128) >> 8;
  }

  // Devolve a pressão em Pa multiplicada por 256 (0 se o cálculo não for possível).
  static uint32_t compensarPressao(const CalibBMP& c, int32_t adcP, int32_t tFine) {
    int64_t v1 = ((int64_t)tFine) - 128000;
    int64_t v2 = v1 * v1 * (int64_t)c.P6;
    v2 = v2 + ((v1 * (int64_t)c.P5) << 17);
    v2 = v2 + (((int64_t)c.P4) << 35);
    v1 = ((v1 * v1 * (int64_t)c.P3) >> 8) + ((v1 * (int64_t)c.P2) << 12);
    v1 = (((((int64_t)1) << 47) + v1)) * ((int64_t)c.P1) >> 33;
    if (v1 == 0) return 0;
    int64_t p = 1048576 - adcP;
    p = (((p << 31) - v2) * 3125) / v1;
    v1 = (((int64_t)c.P9) * (p >> 13) * (p >> 13)) >> 25;
    v2 = (((int64_t)c.P8) * p) >> 19;
    p = ((p + v1 + v2) >> 8) + (((int64_t)c.P7) << 4);
    return (uint32_t)p;
  }

 private:
  static const uint8_t AHT = 0x38;
  bool _aht = false, _bmp = false;
  uint8_t _endBmp = 0x77;
  CalibBMP _cal = {};

  static bool responde(uint8_t end) {
    Wire.beginTransmission(end);
    return Wire.endTransmission() == 0;
  }
  static bool escrever(uint8_t end, const uint8_t* dados, size_t n) {
    Wire.beginTransmission(end);
    for (size_t i = 0; i < n; ++i) Wire.write(dados[i]);
    return Wire.endTransmission() == 0;
  }
  static bool lerBytes(uint8_t end, uint8_t* dest, size_t n) {
    if (Wire.requestFrom(end, n) != n) return false;
    for (size_t i = 0; i < n; ++i) dest[i] = (uint8_t)Wire.read();
    return true;
  }
  static bool lerRegistros(uint8_t end, uint8_t reg, uint8_t* dest, size_t n) {
    Wire.beginTransmission(end);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    return lerBytes(end, dest, n);
  }

  // ----- AHT20 -----
  bool iniciarAHT() {
    if (!responde(AHT)) return false;
    uint8_t estado = 0;
    if (!lerBytes(AHT, &estado, 1)) return false;
    if (!(estado & 0x08)) {                      // ainda não calibrado: manda iniciar
      const uint8_t cmd[3] = { 0xBE, 0x08, 0x00 };
      escrever(AHT, cmd, 3);
      delay(20);
    }
    return true;
  }

  void lerAHT(float& temp, float& umid) {
    const uint8_t cmd[3] = { 0xAC, 0x33, 0x00 };
    if (!escrever(AHT, cmd, 3)) return;
    delay(85);
    uint8_t d[7];
    for (int tentativa = 0; tentativa < 5; ++tentativa) {
      if (!lerBytes(AHT, d, 7)) return;
      if (!(d[0] & 0x80)) break;                 // bit 7 = ainda medindo
      delay(20);
    }
    if (d[0] & 0x80) return;
    uint32_t brutoU = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
    uint32_t brutoT = ((uint32_t)(d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];
    umid = brutoU * 100.0f / 1048576.0f;
    temp = brutoT * 200.0f / 1048576.0f - 50.0f;
  }

  // ----- BMP280 (também aceita o BME280, usando só pressão e temperatura) -----
  bool iniciarBMP(uint8_t end) {
    if (!responde(end)) return false;
    uint8_t id = 0;
    if (!lerRegistros(end, 0xD0, &id, 1) || (id != 0x58 && id != 0x60)) return false;
    uint8_t c[24];
    if (!lerRegistros(end, 0x88, c, 24)) return false;
    auto u16 = [&](int i) { return (uint16_t)(c[i] | (c[i + 1] << 8)); };
    auto s16 = [&](int i) { return (int16_t)u16(i); };
    _cal = { u16(0), s16(2), s16(4), u16(6), s16(8), s16(10), s16(12), s16(14), s16(16), s16(18), s16(20), s16(22) };
    const uint8_t cfg[2] = { 0xF5, 0x00 };       // sem filtro
    escrever(end, cfg, 2);
    _endBmp = end;
    return _cal.T1 != 0 && _cal.P1 != 0;
  }

  void lerBMP(float& temp, float& pressao) {
    const uint8_t medir[2] = { 0xF4, 0x55 };     // temperatura x2, pressão x16, uma medição
    if (!escrever(_endBmp, medir, 2)) return;
    delay(45);
    uint8_t st = 0x08;
    for (int i = 0; i < 10 && (st & 0x08); ++i) {
      if (!lerRegistros(_endBmp, 0xF3, &st, 1)) return;
      if (st & 0x08) delay(5);
    }
    uint8_t d[6];
    if (!lerRegistros(_endBmp, 0xF7, d, 6)) return;
    int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
    if (adcT == 0x80000 || adcP == 0x80000) return;   // valor de "sem medição"
    int32_t tFine;
    temp = compensarTemp(_cal, adcT, tFine) / 100.0f;
    uint32_t p = compensarPressao(_cal, adcP, tFine);
    if (p) pressao = p / 25600.0f;               // Pa*256 -> hPa
  }
};

// Converte a pressão local para o nível do mar, para comparar com a previsão.
inline float pressaoNivelDoMar(float hPa, float altitudeMetros) {
  if (isnan(hPa) || isnan(altitudeMetros)) return NAN;
  return hPa / powf(1.0f - altitudeMetros / 44330.0f, 5.255f);
}
