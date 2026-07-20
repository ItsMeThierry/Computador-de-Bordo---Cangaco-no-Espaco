# Armazenamento na Flash do ESP32

O projeto agora usa a Flash interna como armazenamento principal dos dados de voo. Não depende mais de cartão SD.

Por padrão, o firmware tenta usar a partição customizada `flightlog`. Se ela não existir, cai para EEPROM emulada/NVS.

## Modos

| Modo | Quando usa | Bytes por ponto | Dados salvos |
|---|---|---:|---|
| V1 EEPROM/NVS | Sem partição `flightlog` ou flag desligada | 9 | tempo, estado, altitude, velocidade angular XYZ |
| V2 `flightlog` | Partição `flightlog` disponível | 14 | V1 + aceleração XYZ |

A flag fica em `src/config/Constants.h`:

```cpp
constexpr bool BACKUP_USE_FLIGHTLOG_PARTITION = true;
```

## Formato dos dados

| Campo | Bits | Unidade salva | Resolução |
|---|---:|---|---:|
| Tempo | 16 | centésimos de segundo | `0,01 s` |
| Estado | 3 | código do `FlightState` | `0..7` |
| Altitude barométrica | 16 | decímetros | `0,1 m` |
| Velocidade angular X | 12 | centésimos de rad/s | `0,01 rad/s` |
| Velocidade angular Y | 12 | centésimos de rad/s | `0,01 rad/s` |
| Velocidade angular Z | 12 | centésimos de rad/s | `0,01 rad/s` |
| Aceleração X | 12 | passos de `0,05 m/s²` | `0,05 m/s²` |
| Aceleração Y | 12 | passos de `0,05 m/s²` | `0,05 m/s²` |
| Aceleração Z | 12 | passos de `0,05 m/s²` | `0,05 m/s²` |

No V1, só os primeiros 71 bits são usados:

```txt
71 bits usados + 1 bit livre = 9 bytes
```

No V2, usa os 3 eixos de aceleração:

```txt
107 bits usados + 5 bits livres = 14 bytes
```

## Capacidade

Com a partição `flightlog` de 256 KB:

```txt
262144 bytes totais
- 4096 bytes de metadados
= 258048 bytes para dados
```

Como cada ponto V2 ocupa 14 bytes:

```txt
258048 / 14 = 18432 pontos
```

Tempo aproximado se gravasse tudo em uma taxa fixa:

```txt
50 Hz: 18432 / 50 = 368,64 s = 6 min 8 s
20 Hz: 18432 / 20 = 921,60 s = 15 min 21 s
10 Hz: 18432 / 10 = 1843,20 s = 30 min 43 s
```

No fallback EEPROM/NVS, usando 11776 bytes detectados:

```txt
(11776 - 16) / 9 = 1306 pontos
```

## Frequência

Do início da subida até terminar a região do apogeu:

```txt
50 Hz
1 ponto a cada 20 ms
```

No código, isso inclui:

```txt
ASCENT
ACTIVATE_SKIB_ONE
```

No restante do voo:

```txt
20 Hz
1 ponto a cada 50 ms
```

Estimativa para voo de 2 minutos, com apogeu por volta de 15 s:

```txt
15 s * 50 Hz = 750 pontos
105 s * 20 Hz = 2100 pontos
total = 2850 pontos
```

Mesmo incluindo os 2 s de `ACTIVATE_SKIB_ONE` junto da região crítica:

```txt
17 s * 50 Hz = 850 pontos
103 s * 20 Hz = 2060 pontos
total = 2910 pontos
```

Com 18432 pontos disponíveis no `flightlog`, sobra bastante margem.

Não grava em `PRE_FLIGHT`. Ao entrar em `ASCENT`, limpa o log anterior e começa um voo novo. Ao chegar em `LANDED`, força gravação dos metadados pendentes.

## Sobrescrita espaçada

Primeiro ele preenche todos os espaços:

```txt
0, 1, 2, 3, 4, 5...
```

Quando enche, ele não compacta tudo em bloco. Ele passa a sobrescrever posições espaçadas começando do final para o início, para preservar o começo do voo por mais tempo:

```txt
fator 2: sobrescreve ..., 7, 5, 3, 1
fator 4: sobrescreve ..., 14, 10, 6, 2
fator 8: sobrescreve ..., 28, 20, 12, 4
```

Assim a amostragem efetiva vai diminuindo aos poucos, sem travar o loop fazendo uma compactação gigante.

Na leitura, o CSV já sai ordenado por tempo.

## Limites

Tempo:

```txt
0,00 s até 655,35 s
aprox. 10 min 55 s
```

Altitude:

```txt
0,0 m até 6553,5 m
```

Velocidade angular X, Y e Z:

```txt
-20,48 rad/s até +20,47 rad/s
aprox. -1173 deg/s até +1173 deg/s
```

Aceleração X, Y e Z no V2:

```txt
-102,40 m/s² até +102,35 m/s²
aprox. -10,4 g até +10,4 g
```

Se algum valor passar do limite, ele é saturado no mínimo ou máximo.

## Comandos seriais

Ver status:

```txt
EEPROM_INFO
```

Ler CSV:

```txt
EEPROM_READ
```

Limpar armazenamento:

```txt
EEPROM_CLEAR
```

Preencher com 20 pontos falsos:

```txt
EEPROM_FAKE_SAVE
```

CSV no V1:

```txt
Timestamp,StateCode,State,AltBaro,AngVelX,AngVelY,AngVelZ
```

CSV no V2:

```txt
Timestamp,StateCode,State,AltBaro,AngVelX,AngVelY,AngVelZ,AccX,AccY,AccZ
```

Se o particionamento customizado entrou certo, `EEPROM_INFO` deve mostrar:

```txt
[DataStorage] Backend: flightlog
[DataStorage] Registros: 0/18432 | bytes: 262144 | amostra: 14 | dados em: 4096
```

Se aparecer `EEPROM/NVS`, o firmware caiu no fallback antigo.
