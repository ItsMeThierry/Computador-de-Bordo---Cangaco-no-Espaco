#ifndef CONSTANTS_H
#define CONSTANTS_H

// ========== PARÂMETROS DE VOO — TRANSIÇÕES ==========

// PRE_FLIGHT → ASCENT
// Condição: aceleração vertical >= 2.5g E altitude >= 5m
constexpr float ASCENT_ALTITUDE_THRESHOLD_M   = 5.0f;    // Altitude mínima para decolagem (m)
constexpr float ASCENT_ACCEL_THRESHOLD_G      = 2.5f;    // Aceleração vertical mínima (g)
constexpr float ASCENT_ACCEL_THRESHOLD_MS2    = 24.525f;  // 2.5g convertido para m/s²

// ASCENT → ACTIVATE_SKIB_ONE (detecção de apogeu)
// Condição: velocidade vertical <= 2.5 m/s E -1g <= aceleração vertical <= 1g
constexpr float APOGEE_VERT_VEL_THRESHOLD_MS  = 2.5f;    // Vel vertical <= este valor (m/s)
constexpr float APOGEE_ACCEL_MIN_MS2          = -9.81f;   // -1g em m/s²
constexpr float APOGEE_ACCEL_MAX_MS2          =  9.81f;   // +1g em m/s²

// RECOVERY_STAGE_ONE → ACTIVATE_SKIB_TWO
// Condição: velocidade vertical <= -8 m/s OU altitude <= STAGE_TWO_ALT_THRESHOLD
constexpr float STAGE_TWO_VERT_VEL_THRESHOLD_MS = -8.0f;  // Vel vertical descendente (m/s)
constexpr float STAGE_TWO_ALT_THRESHOLD_M       = 500.0f; // TODO: Ajustar conforme testes da equipe

// RECOVERY_STAGE_TWO → LANDED
// Condição: altitude <= 5m E |velocidade vertical| <= 0.5 m/s
constexpr float LANDED_ALT_THRESHOLD_M         = 5.0f;    // Altitude para considerar pouso (m)
constexpr float LANDED_VERT_VEL_THRESHOLD_MS   = 0.5f;    // Vel vertical abs máxima (m/s)

// Tempo de ativação dos estados SKIB (ms)
constexpr unsigned long SKIB_ACTIVATION_DURATION_MS = 2000;  // 2 segundos

// ========== EIXO VERTICAL DO ACELERÔMETRO ==========
// TODO: Alterar para o eixo correto conforme orientação do MPU no foguete
// 0 = eixo X, 1 = eixo Y, 2 = eixo Z
constexpr int VERTICAL_ACCEL_AXIS = 2;  // Z por padrão

// ========== PARÂMETROS DE SUAVIZAÇÃO ==========
constexpr int   WINDOW_SIZE           = 5;  // Janela da média móvel
constexpr int   ALTITUDE_HISTORY_SIZE = 5;  // Tamanho do histórico de altitudes

// ========== PARÂMETROS DE TEMPORIZAÇÃO ==========
constexpr unsigned long SENSOR_READ_INTERVAL_MS = 50;   // 20 Hz — leitura de sensores

// Gravação EEPROM por estado
constexpr unsigned long EEPROM_INTERVAL_ASCENT_MS       = 50;   // 20 Hz na subida
constexpr unsigned long EEPROM_INTERVAL_RECOVERY_MS     = 200;  // 5 Hz na recuperação

// Gravação SD Card
constexpr unsigned long SD_INTERVAL_MS                  = 10;   // 100 Hz no SD

// Telemetria LoRa
constexpr unsigned long LORA_TX_INTERVAL_MS             = 1000; // 1 Hz LoRa (fixo)

// ========== PARÂMETROS DO SKIB ==========
constexpr unsigned long SKIB_BUZZER_DURATION_MS = 4000;  // Buzzer emergência (ms)

// ========== FEEDBACK: CORES LED RGB POR ESTADO ==========
// Formato: {R, G, B}
// PRE_FLIGHT:          Azul        (0, 0, 255)
// ASCENT:              Ciano       (0, 255, 255)
// ACTIVATE_SKIB_ONE:   Vermelho    (255, 0, 0)
// RECOVERY_STAGE_ONE:  Laranja     (255, 100, 0)
// ACTIVATE_SKIB_TWO:   Vermelho    (255, 0, 0)
// RECOVERY_STAGE_TWO:  Amarelo     (255, 255, 0)
// LANDED:              Verde       (0, 255, 0)

// LED blink intervals (ms) — 0 = sem blink (cor sólida)
constexpr unsigned long LED_BLINK_PRE_FLIGHT_MS       = 1000;
constexpr unsigned long LED_BLINK_ASCENT_MS            = 250;
constexpr unsigned long LED_BLINK_ACTIVATE_SKIB_MS     = 0;     // Sólido (emergência)
constexpr unsigned long LED_BLINK_RECOVERY_ONE_MS      = 1500;
constexpr unsigned long LED_BLINK_RECOVERY_TWO_MS      = 1500;
constexpr unsigned long LED_BLINK_LANDED_MS            = 2000;

// ========== FEEDBACK: BUZZER POR ESTADO ==========
// Frequências (Hz) — 0 = desativado
constexpr unsigned int BEEP_FREQ_PRE_FLIGHT     = 1500;
constexpr unsigned int BEEP_FREQ_ASCENT         = 2093;
constexpr unsigned int BEEP_FREQ_ACTIVATE_SKIB  = 3136;  // Tom contínuo durante SKIB
constexpr unsigned int BEEP_FREQ_RECOVERY_ONE   = 0;     // Sem buzzer
constexpr unsigned int BEEP_FREQ_RECOVERY_TWO   = 0;     // Sem buzzer
constexpr unsigned int BEEP_FREQ_LANDED         = 0;     // Sem buzzer

// Períodos de beep (ms)
constexpr unsigned long BEEP_PERIOD_PRE_FLIGHT_MS = 3000;
constexpr unsigned long BEEP_PERIOD_ASCENT_MS     = 1000;
constexpr unsigned long BEEP_DURATION_MS          = 400;

// ========== PARÂMETROS DE ARMAZENAMENTO ==========
constexpr int   MAX_FLIGHT_DATA_POINTS = 497;   // Pontos na EEPROM
constexpr int   PRE_FLIGHT_QUEUE_SIZE  = 4;     // Pontos pré-voo
constexpr float ALTITUDE_SCALE_FACTOR  = 10.0f; // Para conversão short int

// ========== COMUNICAÇÃO SERIAL ==========
constexpr unsigned long GPS_UART_BAUD  = 9600;   // Baudrate UART GPS NEO-6M
constexpr unsigned long LORA_UART_BAUD = 9600;   // Baudrate UART E220-900T22D

// ========== GPS ==========
constexpr unsigned int GPS_UPDATE_RATE_HZ = 5;   // Taxa de atualização GPS (Hz)

// ========== PINAGEM (ESP32 DevKit V1) ==========
// SKIB
constexpr int PIN_SKIB_1   = 15;   // SKIB primário (paraquedas drogue)
constexpr int PIN_SKIB_2   = 14;  // SKIB secundário (paraquedas principal)

// Feedback
constexpr int PIN_BUZZER    = 13;
constexpr int PIN_LED_RED   = 25;
constexpr int PIN_LED_GREEN = 26;
constexpr int PIN_LED_BLUE  = 27;

// SD Card (SPI)
constexpr int PIN_SD_CS    = 5;
constexpr int PIN_SPI_MOSI = 23;
constexpr int PIN_SPI_MISO = 19;
constexpr int PIN_SPI_SCK  = 18;

// I2C (BMP280 + MPU6050)
constexpr int PIN_I2C_SDA = 21;
constexpr int PIN_I2C_SCL = 22;

// GPS (UART2)
constexpr int PIN_GPS_RX = 36; // TODO: Rever pino do GPS
constexpr int PIN_GPS_TX = 15; // TODO: Rever pino do GPS

// LoRa E220 (UART1)
constexpr int PIN_LORA_RX  = 16;  // TX do módulo → RX do ESP
constexpr int PIN_LORA_TX  = 17;  // RX do módulo → TX do ESP
constexpr int PIN_LORA_M0  = 32;
constexpr int PIN_LORA_M1  = 33;
constexpr int PIN_LORA_AUX = 34;

// ========== COMPATIBILIDADE LEGADA ==========
// Manter para módulos que ainda usam o nome antigo
constexpr int PIN_SKIB = PIN_SKIB_1;

#endif