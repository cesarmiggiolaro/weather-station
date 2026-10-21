// Dados do tempo: estrutura, leitura das respostas do Open-Meteo e textos.
#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

struct Clima {
  bool  valido    = false;   // já recebeu dados pelo menos uma vez
  bool  semSinal  = false;   // a última atualização falhou
  char  cidade[48] = "";
  float temp = NAN, tmax = NAN, tmin = NAN;   // °C
  int   umidade = -1;                         // %
  int   codigo = 0;                           // código WMO do tempo
  bool  dia = true;
  float pressao = NAN;                        // hPa (nível do mar)
  float vento = NAN;                          // km/h
  int   ventoDir = 0;                         // graus
  float uv = NAN;
  int   aqi = -1;                             // índice de qualidade do ar (US AQI)
  long  utcOffset = -10800;                   // fuso em segundos (vem da API)
  float elevacao = NAN;                       // altitude do local em metros (vem da API)

  // Sensor da própria estação (NAN quando não há sensor ligado)
  float casaTemp = NAN;                       // °C
  float casaUmid = NAN;                       // %
  float casaPressao = NAN;                    // hPa, já corrigida para o nível do mar
};

// ---------- leitura do JSON ----------

// Busca de cidade: devolve nome oficial, latitude e longitude.
inline bool lerCidade(const char* json, char* nome, size_t tamNome, float& lat, float& lon) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonVariant r = doc["results"][0];
  if (r.isNull() || r["latitude"].isNull()) return false;
  lat = r["latitude"].as<float>();
  lon = r["longitude"].as<float>();
  snprintf(nome, tamNome, "%s", r["name"] | "");
  return true;
}

// Previsão: condições atuais + máxima e mínima do dia.
inline bool lerPrevisao(const char* json, Clima& c) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonVariant a = doc["current"];
  if (a.isNull() || a["temperature_2m"].isNull()) return false;
  c.temp      = a["temperature_2m"]       | NAN;
  c.umidade   = a["relative_humidity_2m"] | -1;
  c.codigo    = a["weather_code"]         | 0;
  c.dia       = (a["is_day"] | 1) != 0;
  c.pressao   = a["pressure_msl"]         | NAN;
  c.vento     = a["wind_speed_10m"]       | NAN;
  c.ventoDir  = a["wind_direction_10m"]   | 0;
  c.uv        = a["uv_index"]             | NAN;
  c.tmax      = doc["daily"]["temperature_2m_max"][0] | NAN;
  c.tmin      = doc["daily"]["temperature_2m_min"][0] | NAN;
  c.utcOffset = doc["utc_offset_seconds"] | c.utcOffset;
  c.elevacao  = doc["elevation"] | c.elevacao;
  c.valido = true;
  return true;
}

// Qualidade do ar.
inline bool lerAr(const char* json, Clima& c) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonVariant v = doc["current"]["us_aqi"];
  if (v.isNull()) return false;
  c.aqi = v.as<int>();
  return true;
}

// ---------- textos ----------

enum TipoTempo { T_LIMPO, T_POUCAS_NUVENS, T_NUBLADO, T_NEBLINA, T_GAROA, T_CHUVA, T_NEVE, T_TROVOADA };

inline TipoTempo tipoDoCodigo(int cod) {
  if (cod == 0) return T_LIMPO;
  if (cod <= 2) return T_POUCAS_NUVENS;
  if (cod == 3) return T_NUBLADO;
  if (cod == 45 || cod == 48) return T_NEBLINA;
  if (cod >= 51 && cod <= 57) return T_GAROA;
  if ((cod >= 61 && cod <= 67) || (cod >= 80 && cod <= 82)) return T_CHUVA;
  if ((cod >= 71 && cod <= 77) || cod == 85 || cod == 86) return T_NEVE;
  if (cod >= 95) return T_TROVOADA;
  return T_NUBLADO;
}

inline const char* textoDoCodigo(int cod) {
  switch (cod) {
    case 0:  return "Céu limpo";
    case 1:  return "Poucas nuvens";
    case 2:  return "Parcialmente nublado";
    case 3:  return "Nublado";
    case 45: case 48: return "Neblina";
    case 51: case 53: case 55: return "Garoa";
    case 56: case 57: return "Garoa congelante";
    case 61: return "Chuva fraca";
    case 63: return "Chuva";
    case 65: return "Chuva forte";
    case 66: case 67: return "Chuva congelante";
    case 71: case 73: case 75: case 77: case 85: case 86: return "Neve";
    case 80: case 81: return "Pancadas de chuva";
    case 82: return "Pancadas fortes";
    case 95: return "Trovoada";
    case 96: case 99: return "Trovoada com granizo";
  }
  return "";
}

inline const char* direcaoDoVento(int graus) {
  static const char* const nomes[8] = { "N", "NE", "L", "SE", "S", "SO", "O", "NO" };
  int g = ((graus % 360) + 360) % 360;
  return nomes[((g * 2 + 45) / 90) % 8];
}
