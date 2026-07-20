# Particionamento da Flash

Este projeto usa uma tabela customizada em:

```txt
src/partitions.csv
```

Ela reserva uma partição própria para armazenamento de voo:

| Partição | Tamanho | Uso |
|---|---:|---|
| `nvs` | 20 KB | Configurações/NVS normal |
| `factory` | 1664 KB | Código do ESP32 |
| `flightlog` | 256 KB | Armazenamento compacto dos dados de voo |
| `spiffs` | 2112 KB | Espaço restante para arquivos, se precisar |

## Capacidade do flightlog

Cada ponto compacto ocupa 14 bytes na v2 da partição `flightlog`:

```txt
tempo + estado + altitude + velocidade angular X/Y/Z
+ aceleração X/Y/Z
```

Com 256 KB, descontando 4096 bytes de cabeçalho:

```txt
(262144 - 4096) / 14 = 18432 pontos
18432 / 50 Hz = 368,64 s
aprox. 6 min 8 s a 50 Hz
```

## Arduino IDE

Para o upload sempre usar essa tabela:

1. Abra o sketch da pasta `src`.
2. Em `Tools > Partition Scheme`, escolha `Custom`.
3. Faça upload normalmente.

Quando `Partition Scheme` está em `Custom`, o core do ESP32 usa o arquivo `partitions.csv` da pasta do sketch e grava essa tabela junto com o firmware.

No final da compilação, o Arduino IDE deve mostrar um máximo perto de:

```txt
Maximum is 1703936 bytes
```

Se aparecer:

```txt
Maximum is 1310720 bytes
```

ele ainda está usando o particionamento padrão, não o `src/partitions.csv`.

## Terminal com arduino-cli

Use a opção de partição custom no comando de upload/compile. Exemplo:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=custom src
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32:PartitionScheme=custom src
```

O nome da porta pode mudar, por exemplo `/dev/ttyUSB0` ou `/dev/ttyACM0`.

## Importante

Alterar particionamento pode apagar dados antigos da Flash. Depois de trocar a tabela, faça um upload completo e limpe o armazenamento antes do próximo voo.

O código de armazenamento alterna entre os dois modos pela flag em `src/config/Constants.h`:

```cpp
constexpr bool BACKUP_USE_FLIGHTLOG_PARTITION = true;
```

Com `true`, ele tenta usar a partição `flightlog`. Se a partição não existir, ele avisa no Serial e cai no modo antigo `EEPROM/NVS`.

Com `false`, ele usa diretamente o modo antigo `EEPROM/NVS`.

Para verificar no Serial Monitor:

```txt
EEPROM_INFO
```

O retorno deve mostrar:

```txt
[DataStorage] Backend: flightlog
```

Se aparecer `EEPROM/NVS`, o upload ainda não está usando o `partitions.csv` customizado.
