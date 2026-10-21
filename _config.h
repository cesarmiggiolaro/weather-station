// Configuração da estação. É o único arquivo que você precisa editar.
// Nas atualizações do firmware este arquivo não muda: guarde o seu.
#pragma once
#include <stdint.h>

const char* WIFI_NOME  = "";
const char* WIFI_SENHA = "";

const char* CIDADE = "Curitiba";   // nome da cidade
const char* PAIS   = "BR";          // sigla do país (evita cidade de mesmo nome em outro país)

// Só preencha se a cidade não for encontrada pelo nome (0 = procurar pelo nome).
const float LATITUDE  = 0;
const float LONGITUDE = 0;

const int MINUTOS_ENTRE_ATUALIZACOES = 60;

// --- Tela ---
const int PIN_SCL = 7;     // SCL da tela
const int PIN_SDA = 11;    // SDA da tela
const int PIN_CS  = 12;
const int PIN_DC  = 5;
const int PIN_RST = 3;
const int PIN_BL  = 16;    // luz de fundo

const int  ROTACAO        = 0;       // 0 a 3: gire até a imagem ficar em pé na sua montagem
const bool INVERTER_CORES = false;   // mude se as cores aparecerem em negativo
const bool ORDEM_RGB      = true;    // mude se vermelho e azul aparecerem trocados (sol azul)
const uint32_t VELOCIDADE_SPI = 10000000;   // 10 MHz foi testado na placa; com fios curtos pode tentar 20000000

const uint8_t BRILHO_DIA   = 200;    // 0 a 255
const uint8_t BRILHO_NOITE = 50;
const int HORA_INICIO_NOITE = 22;
const int HORA_FIM_NOITE    = 6;

// --- Bateria ---
// Coloque false se não ligou o divisor de resistores no pino da bateria.
const bool  MEDIR_BATERIA   = true;
const int   PIN_BATERIA     = 1;
const float DIVISOR_BATERIA = 2.0f;    // dois resistores iguais (100k + 100k)
const float BATERIA_MINIMA  = 3.50f;   // abaixo disso a estação desliga para proteger a célula
