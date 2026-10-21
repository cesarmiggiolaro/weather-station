# Estação meteorológica

Estação meteorológica com ESP32-S2 Mini e tela redonda de 2,1″ (360×360). Ela busca a previsão no [Open-Meteo](https://open-meteo.com) pelo Wi-Fi e mostra a hora, o tempo agora, a máxima e a mínima do dia, a umidade, o vento e a qualidade do ar. Com um sensor AHT20 + BMP280, mostra também a temperatura e a umidade de dentro de casa. Funciona com bateria 18650 e desliga sozinha quando ela fica fraca.

## O que aparece na tela

- Cidade, data e hora (acertada pela internet)
- Ícone e descrição do tempo, máxima e mínima do dia
- Temperatura e umidade: a de casa em destaque e a de fora menor (sem sensor, só a de fora)
- Vento (velocidade e direção) e qualidade do ar (US AQI)
- Nível da bateria e um aviso quando a última atualização falhou

O brilho acompanha a hora: cheio de dia, 70% a partir das 18h, 40% a partir das 20h e o brilho de noite entre 22h e 6h.

## Peças

| Peça | Para quê |
|---|---|
| LOLIN S2 Mini (ESP32-S2) | controla tudo e conecta no Wi-Fi |
| Tela redonda 2,1″ 360×360, driver GC9B72 | SPI de 4 fios, barra de 10 pinos |
| Placa de carga TP4056 com proteção | carrega a bateria pelo USB-C |
| Bateria 18650 | alimentação |
| Chave liga/desliga | |
| Regulador HT7333 (3,3 V, SOT-89) | baixa a tensão da bateria para os 3,3 V do ESP e da tela |
| 2 capacitores de 10 µF | entrada e saída do HT7333 |
| 1 capacitor de 470 a 1000 µF | segura os picos de consumo do Wi-Fi no 3V3 |
| 2 resistores de 100 kΩ | medição da bateria |
| Sensor AHT20 + BMP280 (opcional) | temperatura, umidade e pressão de casa |

## Ligações

### Tela → ESP32-S2 Mini

| Tela | ESP32-S2 Mini |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | GPIO 7 |
| SDA | GPIO 11 |
| RST | GPIO 3 |
| DC | GPIO 5 |
| CS | GPIO 12 |
| BL | GPIO 16 |
| SDO, TE | não ligar |

### Energia

O HT7333 transforma os 3,0 a 4,2 V da bateria nos 3,3 V que entram direto no pino 3V3 do ESP. A tela e o sensor usam esse mesmo 3V3.

- Placa de carga B+ / B− → bateria + / −
- Placa de carga OUT+ → chave liga/desliga → HT7333 pino 2 (VIN)
- HT7333 pino 3 (VOUT) → pino 3V3 do S2 Mini
- HT7333 pino 1 (GND) e placa de carga OUT− → GND
- 10 µF entre VIN e GND e 10 µF entre VOUT e GND, perto do HT7333
- 470 a 1000 µF entre 3V3 e GND, perto do ESP (perna negativa no GND)
- Medição da bateria: depois da chave e antes do HT7333, 100 kΩ → GPIO 1 → 100 kΩ → GND

A ordem 1 GND, 2 VIN, 3 VOUT é a do HT7333 em SOT-89. Confira na folha de dados do seu.

### Antes de ligar

- Antes de ligar o fio do 3V3 no ESP, ligue a chave e meça o pino 3 do HT7333: tem que dar 3,3 V.
- **Nunca** ligue a bateria (nem o OUT+ da placa de carga) no pino 3V3. Sempre passando pelo HT7333.
- O USB do ESP é só para gravar: desligue a chave antes de plugar o cabo. Com a chave ligada, o regulador da placa e o HT7333 alimentariam o mesmo 3V3.
- Para carregar a bateria, desligue a chave. Com a estação ligada, a placa de carga pode não perceber que a bateria encheu.

### Sensor AHT20 + BMP280 (opcional)

| Sensor | ESP32-S2 Mini |
|---|---|
| VDD | 3V3 (nunca 5 V: o BMP280 aguenta no máximo 3,6 V) |
| GND | GND |
| SDA | GPIO 33 (furo de fora da linha "33/34") |
| SCL | GPIO 35 (furo de fora da linha "35/36") |

Deixe o sensor fora da caixa ou longe do ESP e da tela. Os dois esquentam e fazem a temperatura de casa marcar mais alto. Sem sensor, a estação funciona normalmente e mostra só os dados da previsão.

## Como gravar

1. Na IDE Arduino, instale o suporte **esp32** (Espressif) no Gerenciador de Placas.
2. Instale as bibliotecas **LovyanGFX** (1.2.28 ou mais nova, que tem o driver GC9B72) e **ArduinoJson** (7.x).
3. Crie o seu `config.h` a partir do modelo (veja [Configuração](#configuração)).
4. Abra `estacao_meteo.ino`. Todos os arquivos ficam na mesma pasta.
5. Em **Ferramentas**:
   - Placa: **LOLIN S2 Mini**
   - Partition Scheme: **No OTA (2MB APP/2MB SPIFFS)**
6. Na primeira gravação, segure o botão **0**, aperte **RST** e solte o **0**. Depois de gravar, aperte **RST**.

As mensagens de diagnóstico saem no Monitor Serial a 115200.

## Configuração

O `config.h` guarda o nome e a senha do Wi-Fi, por isso não vai para o repositório (está no `.gitignore`). Copie o modelo e preencha `WIFI_NOME`, `WIFI_SENHA`, `CIDADE` e `PAIS`:

```bash
cp _config.h config.h
```

Nas atualizações do firmware, mantenha o seu `config.h`.

| Constante | Para que serve |
|---|---|
| `WIFI_NOME`, `WIFI_SENHA` | rede Wi-Fi |
| `CIDADE`, `PAIS` | cidade da previsão. `PAIS` é a sigla (ex.: `BR`) e evita uma cidade de mesmo nome em outro país |
| `LATITUDE`, `LONGITUDE` | só se a cidade não for encontrada pelo nome (`0` = procurar pelo nome) |
| `MINUTOS_ENTRE_ATUALIZACOES` | intervalo entre buscas da previsão (padrão: 60) |
| `PIN_SCL`, `PIN_SDA`, `PIN_CS`, `PIN_DC`, `PIN_RST`, `PIN_BL` | pinos da tela |
| `ROTACAO` | 0 a 3: gira a imagem |
| `INVERTER_CORES`, `ORDEM_RGB` | corrigem cores em negativo ou vermelho e azul trocados |
| `VELOCIDADE_SPI` | velocidade da tela (10 MHz testado; com fios curtos dá para tentar 20 MHz) |
| `BRILHO_DIA`, `BRILHO_NOITE` | brilho de 0 a 255 |
| `HORA_INICIO_NOITE`, `HORA_FIM_NOITE` | período do brilho de noite |
| `MEDIR_BATERIA` | `false` se você não ligou o divisor da bateria |
| `PIN_BATERIA`, `DIVISOR_BATERIA` | pino e proporção do divisor (`2.0f` para dois resistores iguais) |
| `BATERIA_MINIMA` | abaixo desta tensão a estação desliga e acorda a cada 10 min para medir de novo |

### Ajustes extras

Estes ajustes não estão no modelo e já têm um valor padrão. Para mudar, acrescente um `#define` no seu `config.h`. Tem que ser `#define`: uma `const` com o mesmo nome é ignorada.

```cpp
#define AJUSTE_TEMPERATURA_CASA -2.0f
```

| Ajuste | Padrão | Para que serve |
|---|---|---|
| `AJUSTE_TEMPERATURA_CASA` | `0.0f` | soma esse valor à temperatura de casa |
| `MINUTOS_ENTRE_LEITURAS_SENSOR` | `30` | intervalo entre leituras do sensor |
| `PIN_I2C_SDA`, `PIN_I2C_SCL` | `33`, `35` | pinos do sensor |
| `HORA_BRILHO_70`, `HORA_BRILHO_40` | `18`, `20` | horas em que o brilho cai para 70% e 40% |
| `MHZ_PROCESSADOR` | `80` | clock do ESP (80 MHz basta e gasta menos que 240) |

## Se algo sair diferente

Ao ligar, a tela mostra a etapa em que está: "Conectando ao Wi-Fi", "Procurando a cidade", "Buscando a previsão" e "Acertando o relógio".

| O que acontece | O que fazer |
|---|---|
| Fica em "Iniciando..." | abra o Monitor Serial e veja a mensagem de erro |
| Tela preta com a luz acesa | confira SCL, SDA, DC, CS e RST |
| Imagem de lado ou de cabeça para baixo | mude `ROTACAO` (0 a 3) |
| Sol azul em vez de amarelo | troque `ORDEM_RGB` |
| Cores em negativo | troque `INVERTER_CORES` |
| Chuvisco ou falhas na imagem | baixe `VELOCIDADE_SPI` para `5000000` e encurte os fios |
| "Sem Wi-Fi" | confira `WIFI_NOME` e `WIFI_SENHA` |
| "Cidade não encontrada" | confira `CIDADE` e `PAIS`, ou preencha `LATITUDE` e `LONGITUDE` |
| "(reiniciou: ...)" na última linha | o ESP reiniciou sozinho, e o texto diz o motivo |
| "(reiniciou: queda de tensão)" | o HT7333 não segurou um pico do Wi-Fi: confira o capacitor de 470 a 1000 µF perto do 3V3 e use fios curtos entre o regulador e o ESP |
| Temperatura de casa alta demais | afaste o sensor do ESP ou use `AJUSTE_TEMPERATURA_CASA` |

## Arquivos

| Arquivo | Conteúdo |
|---|---|
| `estacao_meteo.ino` | programa principal: Wi-Fi, previsão, bateria e configuração da tela |
| `_config.h` | modelo de configuração (copie para `config.h`) |
| `clima.h` | leitura das respostas do Open-Meteo e textos do tempo |
| `tela.h` | desenho da tela, feito em faixas para não piscar e não precisar de PSRAM |
| `sensor.h` | leitura do AHT20 e do BMP280 direto pelo I2C, sem bibliotecas extras |
| `fontes.h` | fonte Inter embutida (gerado automaticamente) |
| `LEIA-ME.txt` | guia de montagem em texto simples |

## Créditos

- Dados do tempo: [Open-Meteo](https://open-meteo.com), gratuito e sem cadastro.
- Fonte: [Inter](https://rsms.me/inter/), licença SIL OFL 1.1.
