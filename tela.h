// Desenho da tela redonda 360x360.
// A imagem é montada em faixas na memória e enviada pronta, para não piscar
// e não depender de PSRAM.
#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <time.h>
#include "clima.h"
#include "fontes.h"

namespace tela {

const int LADO      = 360;
const int ALT_FAIXA = 60;

// ---------- cores (0xRRGGBB) ----------
const uint32_t FUNDO_CIMA  = 0x10284Eu;
const uint32_t FUNDO_BAIXO = 0x050A16u;
const uint32_t CARTAO      = 0x17305Au;
const uint32_t BRANCO      = 0xFFFFFFu;
const uint32_t CLARO       = 0xC9D6EAu;
const uint32_t ROTULO      = 0x9FB2D0u;
const uint32_t AMARELO     = 0xFFC83Du;
const uint32_t LARANJA     = 0xFF9838u;
const uint32_t VERMELHO    = 0xFF5A5Au;
const uint32_t ROXO        = 0xB57BFFu;
const uint32_t VERDE       = 0x4CD080u;
const uint32_t AZUL        = 0x4DB4FFu;
const uint32_t CIANO       = 0x59D8E6u;
const uint32_t NUVEM       = 0xEEF3FAu;
const uint32_t NUVEM_CINZA = 0x8799B5u;
const uint32_t LUA         = 0xF3E7B5u;

// ---------- fontes ----------
struct Fonte {
  lgfx::PointerWrapper dados;
  lgfx::VLWfont vlw;
  void abrir(const uint8_t* p, uint32_t n) { dados.set(p, n); vlw.loadFont(&dados); }
};
static Fonte F13, F17, F22, F34, F92;

static LGFX_Sprite faixa;            // faixa de 360 x ALT_FAIXA
static LovyanGFX*  saida = nullptr;  // para onde as faixas são enviadas
static int OY = 0;                   // linha do topo da faixa atual

inline bool iniciar(LovyanGFX* destino) {
  saida = destino;
  F13.abrir(fonte13, sizeof(fonte13));
  F17.abrir(fonte17, sizeof(fonte17));
  F22.abrir(fonte22, sizeof(fonte22));
  F34.abrir(fonte34, sizeof(fonte34));
  F92.abrir(fonte92, sizeof(fonte92));
  faixa.setColorDepth(16);
  return faixa.createSprite(LADO, ALT_FAIXA) != nullptr;
}

// Chama "desenhar" uma vez por faixa e envia cada faixa para a tela.
template <typename F> void renderizar(F desenhar) {
  if (!saida || !faixa.getBuffer()) return;
  for (OY = 0; OY < LADO; OY += ALT_FAIXA) {
    desenhar();
    faixa.pushSprite(saida, 0, OY);
  }
}

// ---------- primitivas ----------
using lgfx::textdatum_t;

inline uint32_t mistura(uint32_t a, uint32_t b, int t, int total) {
  int r = ((a >> 16) & 255) + (((int)((b >> 16) & 255) - (int)((a >> 16) & 255)) * t) / total;
  int g = ((a >> 8) & 255)  + (((int)((b >> 8) & 255)  - (int)((a >> 8) & 255))  * t) / total;
  int v = (a & 255)         + (((int)(b & 255)         - (int)(a & 255))         * t) / total;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)v;
}
inline uint32_t corFundo(int y) { return mistura(FUNDO_CIMA, FUNDO_BAIXO, y, LADO - 1); }

// Degradê do fundo com pontilhado, para não aparecerem degraus de cor na tela de 16 bits.
inline void fundo() {
  static const uint8_t BAYER[4][4] = { {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5} };
  uint16_t* p = (uint16_t*)faixa.getBuffer();
  for (int y = 0; y < ALT_FAIXA; ++y) {
    uint32_t c = corFundo(y + OY);
    int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    uint16_t padrao[4];
    for (int i = 0; i < 4; ++i) {
      int d = BAYER[(y + OY) & 3][i];                       // 0..15
      int r5 = (r * 31 * 16 / 255 + d) >> 4;
      int g6 = (g * 63 * 16 / 255 + d) >> 4;
      int b5 = (b * 31 * 16 / 255 + d) >> 4;
      uint16_t v = (uint16_t)((r5 << 11) | (g6 << 5) | b5);
      padrao[i] = (uint16_t)((v >> 8) | (v << 8));          // a faixa guarda os bytes invertidos
    }
    for (int x = 0; x < LADO; ++x) *p++ = padrao[x & 3];
  }
}
inline void texto(const char* s, int x, int y, Fonte& f, uint32_t cor,
                  textdatum_t alinhamento = textdatum_t::baseline_left) {
  if (y - OY < -20 || y - OY > ALT_FAIXA + 60) return;   // fora desta faixa
  faixa.setFont(&f.vlw);
  faixa.setTextColor(cor);
  faixa.setTextDatum(alinhamento);
  faixa.drawString(s, x, y - OY);
}
inline int largura(const char* s, Fonte& f) {
  faixa.setFont(&f.vlw);
  return faixa.textWidth(s);
}
inline void circulo(int x, int y, int r, uint32_t cor) { faixa.fillSmoothCircle(x, y - OY, r, cor); }
inline void linha(float x0, float y0, float x1, float y1, float meiaEspessura, uint32_t cor) {
  faixa.drawWideLine((int)lroundf(x0), (int)lroundf(y0) - OY, (int)lroundf(x1), (int)lroundf(y1) - OY, meiaEspessura, cor);
}
inline void cunha(int x0, int y0, int x1, int y1, float r0, float r1, uint32_t cor) {
  faixa.drawWedgeLine(x0, y0 - OY, x1, y1 - OY, r0, r1, cor);
}
inline void cartao(int x, int y, int w, int h) { faixa.fillSmoothRoundRect(x, y - OY, w, h, 12, CARTAO); }

// Troca o ponto decimal por vírgula.
inline void numero(char* buf, size_t tam, float v, int casas) {
  if (isnan(v)) { snprintf(buf, tam, "--"); return; }
  snprintf(buf, tam, "%.*f", casas, (double)v);
  for (char* p = buf; *p; ++p) if (*p == '.') *p = ',';
}

// ---------- ícones ----------
inline void sol(int cx, int cy, int r, int raio, float esp, int folga = 5) {
  for (int i = 0; i < 8; ++i) {
    float a = i * 0.7853982f;
    linha(cx + cosf(a) * (r + folga), cy + sinf(a) * (r + folga), cx + cosf(a) * raio, cy + sinf(a) * raio, esp, AMARELO);
  }
  circulo(cx, cy, r, AMARELO);
}
inline void lua(int cx, int cy, int r) {
  circulo(cx, cy, r, LUA);
  circulo(cx + (r * 9) / 20, cy - (r * 7) / 20, (r * 17) / 20, corFundo(cy));
}
inline void nuvem(int cx, int cy, float s, uint32_t cor) {
  circulo(cx - (int)(15 * s), cy + (int)(4 * s), (int)(12 * s), cor);
  circulo(cx + (int)(1 * s),  cy - (int)(4 * s), (int)(17 * s), cor);
  circulo(cx + (int)(17 * s), cy + (int)(5 * s), (int)(11 * s), cor);
  faixa.fillRect(cx - (int)(15 * s), cy + (int)(4 * s) - OY, (int)(32 * s), (int)(12 * s) + 1, cor);
}
inline void gota(int cx, int cy, int alt, uint32_t cor) {
  float r = alt * 0.34f;
  cunha(cx, cy - alt / 2, cx, cy + alt / 2 - (int)r, 0.6f, r, cor);
}
inline void seta(int x, int y, bool paraCima, uint32_t cor) {
  int ponta = paraCima ? y - 6 : y + 6;
  int lado  = paraCima ? y - 1 : y + 1;
  linha(x, y + (paraCima ? 6 : -6), x, ponta, 1.0f, cor);
  linha(x - 4, lado, x, ponta, 1.0f, cor);
  linha(x + 4, lado, x, ponta, 1.0f, cor);
}
inline void iconeVento(int cx, int cy, uint32_t cor) {
  linha(cx - 10, cy - 4, cx + 3, cy - 4, 1.0f, cor);
  faixa.fillArc(cx + 3, cy - 8 - OY, 3, 5, 270, 450, cor);
  linha(cx - 10, cy + 1, cx + 8, cy + 1, 1.0f, cor);
  linha(cx - 6,  cy + 6, cx + 2, cy + 6, 1.0f, cor);
  faixa.fillArc(cx + 2, cy + 10 - OY, 3, 5, 270, 450, cor);
}
inline void iconeMedidor(int cx, int cy, uint32_t cor) {
  faixa.fillArc(cx, cy - OY, 8, 10, 150, 390, cor);
  linha(cx, cy, cx + 4, cy - 5, 1.0f, BRANCO);
  circulo(cx, cy, 2, BRANCO);
}
inline void iconeLocal(int cx, int cy, uint32_t cor) {
  cunha(cx, cy + 8, cx, cy - 2, 0.6f, 6.0f, cor);
  circulo(cx, cy - 2, 2, corFundo(cy));
}
inline void iconeCasa(int x, int y, uint32_t cor) {   // (x, y) = canto superior esquerdo, 14 x 13
  faixa.fillTriangle(x, y + 6 - OY, x + 7, y - OY, x + 14, y + 6 - OY, cor);
  faixa.fillRect(x + 2, y + 6 - OY, 11, 7, cor);
  faixa.fillRect(x + 6, y + 9 - OY, 3, 4, corFundo(y + 11));
}
inline void iconeBateria(int x, int y, int pct) {
  uint32_t cor = pct > 40 ? VERDE : (pct > 15 ? AMARELO : VERMELHO);
  faixa.fillSmoothRoundRect(x, y - OY, 24, 12, 3, CLARO);
  faixa.fillRect(x + 2, y + 2 - OY, 20, 8, corFundo(y));
  faixa.fillRect(x + 24, y + 4 - OY, 2, 4, CLARO);
  int w = (18 * pct + 50) / 100;
  if (w < 2) w = 2;
  faixa.fillRect(x + 3, y + 3 - OY, w, 6, cor);
}

// Ícone do tempo centrado em (cx, cy). Em escala 1 ocupa uns 80 px de largura.
inline void iconeTempo(int cx, int cy, int codigo, bool dia, float e = 1.0f) {
  auto E = [e](float v) { return (int)lroundf(v * e); };
  switch (tipoDoCodigo(codigo)) {
    case T_LIMPO:
      if (dia) sol(cx, cy, E(21), E(36), 2.0f * e, E(5)); else lua(cx, cy, E(24));
      break;
    case T_POUCAS_NUVENS:
      if (dia) sol(cx + E(16), cy - E(12), E(16), E(28), 1.8f * e, E(5)); else lua(cx + E(16), cy - E(12), E(17));
      nuvem(cx - E(6), cy + E(10), 1.1f * e, NUVEM);
      break;
    case T_NUBLADO:
      nuvem(cx + E(14), cy - E(8), 0.85f * e, NUVEM_CINZA);
      nuvem(cx - E(6), cy + E(10), 1.1f * e, NUVEM);
      break;
    case T_NEBLINA:
      nuvem(cx, cy - E(8), 1.1f * e, NUVEM);
      for (int i = 0; i < 2; ++i) linha(cx - E(26 - i * 8), cy + E(20 + i * 9), cx + E(22 + i * 4), cy + E(20 + i * 9), 1.6f * e, CLARO);
      break;
    case T_GAROA:
    case T_CHUVA: {
      nuvem(cx, cy - E(8), 1.1f * e, NUVEM);
      int n = tipoDoCodigo(codigo) == T_CHUVA ? 4 : 3;
      for (int i = 0; i < n; ++i) {
        float x = cx + (-(n - 1) * 7 + i * 14) * e;
        linha(x + 3 * e, cy + 19 * e, x - 2 * e, cy + 31 * e, 1.6f * e, AZUL);
      }
      break;
    }
    case T_NEVE:
      nuvem(cx, cy - E(8), 1.1f * e, NUVEM);
      for (int i = 0; i < 4; ++i) circulo(cx + E(-21 + i * 14), cy + E(23 + (i & 1) * 8), E(3) < 1 ? 1 : E(3), BRANCO);
      break;
    case T_TROVOADA:
      nuvem(cx, cy - E(8), 1.1f * e, NUVEM_CINZA);
      faixa.fillTriangle(cx + E(4), cy + E(8) - OY, cx - E(8), cy + E(24) - OY, cx + E(1), cy + E(24) - OY, AMARELO);
      faixa.fillTriangle(cx - E(1), cy + E(21) - OY, cx + E(8), cy + E(21) - OY, cx - E(5), cy + E(38) - OY, AMARELO);
      break;
  }
}

// ---------- cartões ----------
// Cartão com rótulo, ícone, valor, unidade pequena e (opcional) um estado embaixo.
enum IconeCartao { IC_GOTA_AZUL, IC_GOTA_VERDE, IC_SOL, IC_VENTO, IC_MEDIDOR };

inline void cartaoValor(int x, int y, int w, int h, const char* rotulo, IconeCartao icone,
                        const char* valor, const char* unidade, const char* estado, uint32_t corEstado) {
  if (y + h < OY || y > OY + ALT_FAIXA) return;
  cartao(x, y, w, h);
  texto(rotulo, x + w / 2, y + 17, F13, ROTULO, textdatum_t::baseline_center);

  int lv = largura(valor, F22);
  int lu = unidade[0] ? largura(unidade, F13) + 3 : 0;
  int total = 20 + 7 + lv + lu;
  int x0 = x + (w - total) / 2;
  int base = estado ? y + 38 : y + h - 14;      // linha de base do valor
  int ci = x0 + 10, yi = base - 8;              // centro do ícone
  switch (icone) {
    case IC_GOTA_AZUL:  gota(ci, yi, 20, AZUL);  break;
    case IC_GOTA_VERDE: gota(ci, yi, 20, corEstado); break;
    case IC_SOL:        sol(ci, yi, 4, 10, 0.9f); break;
    case IC_VENTO:      iconeVento(ci, yi, CIANO); break;
    case IC_MEDIDOR:    iconeMedidor(ci, yi + 1, CIANO); break;
  }
  texto(valor, x0 + 27, base, F22, BRANCO);
  if (lu) texto(unidade, x0 + 27 + lv + 3, base, F13, CLARO);
  if (estado) texto(estado, x + w / 2, y + h - 6, F13, corEstado, textdatum_t::baseline_center);
}

// Cartão grande: o valor medido em casa em fonte grande e, ao lado, o valor de fora (previsão) menor.
// Sem sensor, "grande" é o valor de fora e "pequeno" vem vazio.
inline void cartaoDuplo(int x, int y, int w, int h, const char* rotulo, bool comCasa,
                        const char* grande, const char* pequeno) {
  if (y + h < OY || y > OY + ALT_FAIXA) return;
  cartao(x, y, w, h);
  int lr = largura(rotulo, F13);
  int xr = x + w / 2 - (lr + (comCasa ? 18 : 0)) / 2;
  if (comCasa) iconeCasa(xr, y + 8, ROTULO);
  texto(rotulo, xr + (comCasa ? 18 : 0), y + 19, F13, ROTULO);

  int lg = largura(grande, F34);
  int lp = (pequeno && pequeno[0]) ? (int)fmaxf(largura("fora", F13), largura(pequeno, F17)) : 0;
  int total = lg + (lp ? 14 + lp : 0);
  int x0 = x + (w - total) / 2;
  texto(grande, x0, y + h - 12, F34, BRANCO);
  if (lp) {
    int xc = x0 + lg + 14 + lp / 2;
    texto("fora", xc, y + h - 31, F13, ROTULO, textdatum_t::baseline_center);
    texto(pequeno, xc, y + h - 12, F17, CLARO, textdatum_t::baseline_center);
  }
}

inline const char* estadoAr(int aqi, uint32_t& cor) {
  if (aqi < 0)    { cor = ROTULO;   return ""; }
  if (aqi <= 50)  { cor = VERDE;    return "Boa"; }
  if (aqi <= 100) { cor = AMARELO;  return "Moderada"; }
  if (aqi <= 150) { cor = LARANJA;  return "Ruim"; }
  if (aqi <= 200) { cor = VERMELHO; return "Insalubre"; }
  if (aqi <= 300) { cor = ROXO;     return "Muito ruim"; }
  cor = VERMELHO; return "Perigosa";
}
inline const char* estadoUV(float uv, uint32_t& cor) {
  if (isnan(uv))  { cor = ROTULO;   return ""; }
  if (uv < 2.5f)  { cor = VERDE;    return "Baixo"; }
  if (uv < 5.5f)  { cor = AMARELO;  return "Moderado"; }
  if (uv < 7.5f)  { cor = LARANJA;  return "Alto"; }
  if (uv < 10.5f) { cor = VERMELHO; return "Muito alto"; }
  cor = ROXO; return "Extremo";
}

// ---------- telas ----------

// Tela principal. "agora" pode ser nullptr (hora ainda não sincronizada).
// bateria: 0 a 100, ou -1 para não mostrar.
inline void principal(const Clima& c, const struct tm* agora, int bateria) {
  static const char* const DIAS[7]   = { "Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb" };
  static const char* const MESES[12] = { "jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez" };
  char b[48], b2[16];

  renderizar([&]() {
    fundo();

    // cidade
    int lc = largura(c.cidade, F17);
    int xc = LADO / 2 - (lc + 18) / 2;
    iconeLocal(xc + 6, 30, BRANCO);
    texto(c.cidade, xc + 18, 38, F17, BRANCO);

    // data e hora (hora bem grande)
    if (agora) {
      snprintf(b, sizeof(b), "%s, %d de %s", DIAS[agora->tm_wday % 7], agora->tm_mday, MESES[agora->tm_mon % 12]);
      texto(b, LADO / 2, 62, F17, CLARO, textdatum_t::baseline_center);
      snprintf(b, sizeof(b), "%02d:%02d", agora->tm_hour, agora->tm_min);
    } else {
      snprintf(b, sizeof(b), "--:--");
    }
    texto(b, LADO / 2, 140, F92, BRANCO, textdatum_t::baseline_center);

    // tempo agora numa linha: ícone pequeno, descrição, máxima e mínima
    {
      const char* desc = textoDoCodigo(c.codigo);
      char smax[12], smin[12];
      numero(b2, sizeof(b2), c.tmax, 0); snprintf(smax, sizeof(smax), "%s°", b2);
      numero(b2, sizeof(b2), c.tmin, 0); snprintf(smin, sizeof(smin), "%s°", b2);
      int ld = largura(desc, F17), lmax = largura(smax, F17), lmin = largura(smin, F17);
      int total = 44 + ld + 16 + (12 + lmax) + 10 + (12 + lmin);
      int x = LADO / 2 - total / 2;
      iconeTempo(x + 20, 161, c.codigo, c.dia, 0.5f);
      texto(desc, x + 44, 169, F17, CLARO);
      x += 44 + ld + 16;
      seta(x + 4, 163, true, VERMELHO);
      texto(smax, x + 12, 169, F17, BRANCO);
      x += 12 + lmax + 10;
      seta(x + 4, 164, false, AZUL);
      texto(smin, x + 12, 169, F17, BRANCO);
    }

    // primeira linha: temperatura e umidade (casa em destaque, fora menor)
    bool casaT = !isnan(c.casaTemp), casaU = !isnan(c.casaUmid);
    char grande[16], pequeno[16];
    numero(b2, sizeof(b2), casaT ? c.casaTemp : c.temp, 1);
    snprintf(grande, sizeof(grande), "%s°", b2);
    pequeno[0] = 0;
    if (casaT) { numero(b2, sizeof(b2), c.temp, 1); snprintf(pequeno, sizeof(pequeno), "%s°", b2); }
    cartaoDuplo(24, 186, 154, 68, "Temperatura", casaT, grande, pequeno);

    int umidGrande = casaU ? (int)lroundf(c.casaUmid) : c.umidade;
    if (umidGrande >= 0) snprintf(grande, sizeof(grande), "%d%%", umidGrande); else snprintf(grande, sizeof(grande), "--");
    pequeno[0] = 0;
    if (casaU) {
      if (c.umidade >= 0) snprintf(pequeno, sizeof(pequeno), "%d%%", c.umidade); else snprintf(pequeno, sizeof(pequeno), "--");
    }
    cartaoDuplo(182, 186, 154, 68, "Umidade", casaU, grande, pequeno);

    // segunda linha: vento e qualidade do ar
    uint32_t cor;
    snprintf(b2, sizeof(b2), "Vento · %s", direcaoDoVento(c.ventoDir));
    numero(b, sizeof(b), c.vento, 0);
    cartaoValor(68, 258, 110, 60, isnan(c.vento) ? "Vento" : b2, IC_VENTO, b, "km/h", nullptr, CIANO);
    const char* e = estadoAr(c.aqi, cor);
    if (c.aqi >= 0) snprintf(b, sizeof(b), "%d", c.aqi); else snprintf(b, sizeof(b), "--");
    cartaoValor(182, 258, 110, 60, "Qualid. do ar", IC_GOTA_VERDE, b, "", e, cor);

    // rodapé: bateria e aviso de conexão
    char bb[12] = "";
    const char* aviso = c.semSinal ? "sem conexão" : "";
    int la = aviso[0] ? largura(aviso, F13) : 0, lBat = 0;
    if (bateria >= 0) {
      snprintf(bb, sizeof(bb), "%d%%", bateria);
      lBat = 26 + 5 + largura(bb, F13);
    }
    int totalRod = lBat + la + (lBat && la ? 10 : 0);
    int xr = LADO / 2 - totalRod / 2;
    if (lBat) {
      iconeBateria(xr, 327, bateria);
      texto(bb, xr + 31, 338, F13, CLARO);
      xr += lBat + 10;
    }
    if (la) texto(aviso, xr, 338, F13, LARANJA);
  });
}

// Tela de aviso com um título e até duas linhas de texto.
inline void mensagem(const char* titulo, const char* linha1, const char* linha2 = "") {
  renderizar([&]() {
    fundo();
    texto(titulo, LADO / 2, 168, F22, BRANCO, textdatum_t::baseline_center);
    texto(linha1, LADO / 2, 200, F17, CLARO, textdatum_t::baseline_center);
    texto(linha2, LADO / 2, 224, F17, CLARO, textdatum_t::baseline_center);
  });
}

}  // namespace tela
