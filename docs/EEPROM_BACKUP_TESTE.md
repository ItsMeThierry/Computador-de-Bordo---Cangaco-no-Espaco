# Backup na Flash do ESP32

Este backup usa a Flash interna do ESP32 via EEPROM emulada/NVS. Não é uma EEPROM física dedicada.

## O que é salvo

Cada ponto de voo salva:

| Campo | Tipo | Bytes | Unidade salva | Resolução |
|---|---:|---:|---|---:|
| Tempo | `uint16_t` | 2 | centésimos de segundo | `0,01 s` |
| Altitude barométrica | `uint16_t` | 2 | decímetros | `0,1 m` |
| Aceleração X | `int16_t` | 2 | centésimos de m/s² | `0,01 m/s²` |
| Aceleração Y | `int16_t` | 2 | centésimos de m/s² | `0,01 m/s²` |
| Aceleração Z | `int16_t` | 2 | centésimos de m/s² | `0,01 m/s²` |

Total por ponto:

```txt
10 bytes
```

O cabeçalho ocupa:

```txt
16 bytes
```

Ele guarda assinatura, versão, quantidade de registros, fator de decimação, contador bruto e próximo índice de sobrescrita.

## Frequência de gravação

Na subida:

```txt
20 Hz
1 ponto a cada 50 ms
```

Na recuperação:

```txt
5 Hz
1 ponto a cada 200 ms
```

Não grava em `PRE_FLIGHT` nem em `LANDED`.

As amostras são acumuladas na RAM da EEPROM emulada e o `EEPROM.commit()` é feito em lote a cada 20 amostras. Ao chegar em `LANDED`, ou ao usar comandos seriais como `EEPROM_READ` e `EEPROM_CLEAR`, qualquer lote pendente é gravado antes de continuar.

## Capacidade

A capacidade depende do espaço detectado na partição NVS. A fórmula é:

```txt
pontos = (bytes_disponiveis - 16) / 10
```

No teste real da ESP32 conectada, a área detectada foi:

```txt
11776 bytes
```

Então:

```txt
(11776 - 16) / 10 = 1176 pontos
```

Com 20 Hz constantes:

```txt
1176 / 20 = 58,8 s antes da primeira sobrescrita espaçada
```

## Lógica de sobrescrita espaçada

O sistema primeiro preenche todos os espaços disponíveis:

```txt
0, 1, 2, 3, 4, 5...
```

Quando enche, ele não compacta tudo de uma vez. Em vez disso, passa a sobrescrever posições espaçadas.

Primeira rodada cheia:

```txt
fator 2
sobrescreve 1, 3, 5, 7...
```

Depois que termina essa rodada:

```txt
fator 4
sobrescreve 2, 6, 10, 14...
```

Depois:

```txt
fator 8, 16...
```

Isso reduz a amostragem efetiva aos poucos, mantendo uma visão geral do voo sem fazer uma compactação em bloco, que poderia travar ou atrasar o loop.

## Limites dos campos

Tempo:

```txt
0,00 s até 655,35 s
aprox. 10 min 55 s
```

Altitude:

```txt
0,0 m até 6553,5 m
```

Aceleração em cada eixo:

```txt
-327,68 m/s² até +327,67 m/s²
aprox. -33,4 g até +33,4 g
```

Se algum valor passar do limite, ele é saturado no mínimo ou máximo.

## Leitura dos dados

O comando serial:

```txt
EEPROM_READ
```

imprime CSV ordenado por tempo:

```txt
Timestamp,AltBaro,AccX,AccY,AccZ
```

Mesmo após a sobrescrita espaçada misturar a ordem física dos registros na Flash, a leitura carrega os pontos em RAM, ordena por `Timestamp` e imprime em ordem cronológica.

O comando:

```txt
EEPROM_INFO
```

mostra quantidade de registros, capacidade, bytes usados, fator de decimação e próximo índice de sobrescrita.

O comando:

```txt
EEPROM_CLEAR
```

zera o backup.

## Teste de bancada

Para testar sem depender da detecção de voo, use o comando serial:

```txt
EEPROM_FAKE_SAVE
```

Ele grava 20 amostras artificiais diretamente no backup.

Fluxo sugerido no Monitor Serial, em `115200` baud e com quebra de linha habilitada:

```txt
EEPROM_CLEAR
EEPROM_INFO
EEPROM_FAKE_SAVE
EEPROM_INFO
EEPROM_READ
```

Depois de `EEPROM_FAKE_SAVE`, o `EEPROM_INFO` deve mostrar 20 registros a mais, e o `EEPROM_READ` deve imprimir o CSV com essas amostras.
