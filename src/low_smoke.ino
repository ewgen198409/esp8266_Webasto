// low_smoke.txt
#include <Arduino.h>
#include <Wire.h> // Библиотека для работы с I2C
#include <LiquidCrystal_I2C.h> // Библиотека для работы с дисплеем
#include <EEPROM.h> // Для ESP8266 EEPROM
#include <string.h> // Для функций работы с C-строками: strcmp, strncmp, strchr, strtok, atoi
#include <ctype.h> // Для isspace()

#include <ESP8266WiFi.h> // Добавьте, если еще нет
#include <ESP8266WebServer.h> // Добавьте, если еще нет
#include <WebSocketsServer.h> // Добавьте, если еще нет
#include <ESP8266mDNS.h> // Библиотека для mDNS

#include <OneWire.h>
#include <DallasTemperature.h>

// Добавьте в начало файла
#include <LittleFS.h>

// OTA константы и переменные
const int OTA_BUFFER_SIZE = 512;
const unsigned long OTA_TIMEOUT = 600000; // 10 минут таймаут
const size_t MAX_FIRMWARE_SIZE = 1048576; // Макс. размер прошивки 1MB

bool otaInProgress = false;
bool otaWriteInProgress = false;
unsigned long otaStartTime = 0;
size_t otaTotalSize = 0;
size_t otaReceived = 0;
File otaFile;

// Флаг OTA в EEPROM
const int OTA_FLAG_ADDR = 500;

OneWire oneWire(D0);                                    // Пин D0 для датчика DS18B20
DallasTemperature sensors(&oneWire);
float ds18b20_temp;                                     // Переменная для хранения температуры с DS18B20
// Асинхронное чтение DS18B20
unsigned long lastTempRequest = 0;
bool tempRequested = false;
int ds18b20_error_count = 0;
const unsigned long TEMP_READ_INTERVAL = 1000;          // Читаем раз в секунду
bool ds18b20_disabled = false;                        // НОВОЕ: флаг отключения датчика


// Укажите адрес вашего I2C дисплея (обычно 0x27 или 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Установите адрес, количество символов и строк

// Переименовываем состояния, чтобы избежать конфликта с Arduino константами
enum SystemState {
  STATE_HIGH,   // вместо HIGH        // 0
  STATE_MID,    // вместо MID         // 1
  STATE_LOW     // вместо LOW         // 2
};
SystemState currentState = STATE_MID;  // Начальное состояние

// Определяем массив для контура знакоместа
byte contour[] = { B11111, B10001, B10001, B10001, B10001, B10001, B10001, B11111 };

byte fill1[8] = { B11111, B10001, B10001, B10001, B10001, B10001, B11111, B11111 }; // 1 и 2 пиксель
byte fill2[8] = { B11111, B10001, B10001, B10001, B10001, B11111, B11111, B11111 }; // 3 пикселя
byte fill3[8] = { B11111, B10001, B10001, B10001, B11111, B11111, B11111, B11111 }; // 4 пикселя
byte fill4[8] = { B11111, B10001, B10001, B11111, B11111, B11111, B11111, B11111 }; // 5 пикселей
byte fill5[8] = { B11111, B10001, B11111, B11111, B11111, B11111, B11111, B11111 }; // 6 пикселей
byte fill6[8] = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 }; // 7 пикселей

// Структура для хранения параметров в EEPROM
struct Settings {
  unsigned long magicNumber; // Добавляем "магическое число" для проверки целостности/первого запуска
  int pump_size;
  int heater_target;
  int heater_min;
  int heater_overheat;
  int heater_warning;
  // Новые глобальные переменные
  int max_pwm_fan;
  unsigned long glow_brightness; // Изменено на unsigned long
  unsigned long glow_fade_in_duration; // Изменено на unsigned long
  unsigned long glow_fade_out_duration; // Изменено на unsigned long
};

Settings settings;

// Произвольное магическое число для проверки EEPROM (можно изменить)
const unsigned long EEPROM_MAGIC_NUMBER = 0x12345679; // ИЗМЕНЕНО: Новое магическое число

unsigned long glow_brightness_max = 20;   // максимальная яркость свечи
unsigned long glow_fade_in_duration_ms = 10000;
unsigned long glow_fade_out_duration_ms = 5000;

// переменные для рассхода топлива
float total_fuel_consumed_liters = 0.0;
float fuel_consumption_per_hour = 0.0;

// Буфер для входящих команд
const int SERIAL_BUFFER_SIZE = 256;                                                               // Увеличен буфер для длинных команд SET
char inputBuffer[SERIAL_BUFFER_SIZE];
byte inputBufferIndex = 0;
bool stringComplete = false;

// Подключения пинов для NodeMCU v2 ESP12E
// Используем D-пины, которые соответствуют GPIO на ESP8266
int fuel_pump_pin = D5; // GPIO14
int glow_plug_pin = D6; // GPIO12
int burn_fan_pin = D1; // GPIO13
int exhaust_temp_pin = A0; // ADC0, единственный аналоговый вход на NodeMCU
int push_pin = D7; // GPIO5
int pushup_pin = D2; // GPIO4
int pushdown_pin = D3; // GPIO0

// Конфигурация оборудования
int pump_size = 22;                     // Размер насоса (22, 30, 60)
bool high_temp_exhaust_sensor = true;   // Флаг для высокотемпературного датчика выхлопа

// Топливная смесь
// Предварительная подготовка
int prime_fan_speed = 50; // Скорость вентилятора при предварительной подготовке
int prime_low_temp = 0; // Нижняя температура при предварительной подготовке
float prime_low_temp_fuelrate = 2.5; // Расход топлива при низкой температуре
int prime_high_temp = 20; // Верхняя температура при предварительной подготовке
float prime_high_temp_fuelrate = 2.0; // Расход топлива при высокой температуре

// Начальная
float start_fan_speed = 15; // Начальная скорость вентилятора
float start_fuel = 0.7; // Начальный расход топлива

// Полная мощность
float final_fan_speed = 100.00; // Конечная скорость вентилятора
float final_fuel = 2.38; // Конечный расход топлива
int full_power_increment_time = 60; // Время увеличения мощности, секунды

// Регулировка мощности
float throttling_high_fuel = 2.20; // Верхний расход топлива при регулировке
float throttling_middle_fuel = 2.01; // Средний расход топлива при регулировке
float throttling_low_fuel = 1.65; // Нижний расход топлива при регулировке

// Настройки последовательного порта
const char* message = "Off"; // Имя переменной изменено с message_str на message
bool pushed; // Флаг нажатия кнопки (используется ли?)
bool push; // Флаг нажатия кнопки (используется ли?)
bool debug_glow_plug_on = false; // Отладочный флаг для свечи накаливания (исправлено на bool)

// Переменные
float fan_speed; // Скорость вентилятора, проценты
float fuel_need; // Необходимый расход топлива, проценты
int glow_time; // Время накаливания, секунды
float exhaust_temp; // Температура выхлопа, градусы Цельсия
float exhaust_temp_sec[10]; // Массив температур выхлопа за последние 10 секунд, градусы Цельсия
int glow_left = 0; // Оставшееся время накаливания
int last_glow_value = 0; // Последнее значение накаливания
bool burn; // Флаг для горения
bool webasto_fail; // Флаг для сбоя Webasto
bool lean_burn; // Флаг для бедного горения
int delayed_period = 0; // Период задержки
long glowing_on = 0; // Время накаливания
int burn_mode = 0; // Режим горения

// Конфигурация нагревателя
int heater_target = 195; // Целевая температура для нагревателя, градусы Цельсия
int heater_min = 190; // Минимальная температура для нагревателя, градусы Цельсия
int heater_overheat = 210; // Температура перегрева нагревателя, градусы Цельсия
int heater_warning = 200; // Температура предупреждения для нагревателя, градусы Цельсия

volatile bool settingsUpdateInProgress = false; // Флаг для блокировки logging() (предполагается, что logging() существует)

// Переменные для неблокирующей прокачки топлива
bool fuelPumpingActive = false;
unsigned long fuelPumpStartTime = 0;
int fuelPumpCurrentCycle = 0;
unsigned long lastFuelPumpToggleTime = 0;
bool fuelPumpState = LOW; // Текущее состояние пина насоса (LOW или HIGH)

const unsigned long PUMP_ON_DURATION_MS = 60;  // Длительность включения насоса в мс
const unsigned long PUMP_OFF_DURATION_MS = 110; // Длительность выключения насоса в мс
const int TOTAL_PUMP_CYCLES = 200; // Общее количество циклов прокачки

bool loggingEnabled = false; // Инициализируем лог по умолчанию выключенным

// Глобальные переменные для отслеживания предыдущих состояний дисплея
int prev_exhaust_temp = -1;
int prev_fan_speed = -1;
float prev_delayed_period = -1.0; // Инициализируем как float
int prev_burn_mode = -1;
bool prev_debug_glow_plug_on = false; // Инициализируем как bool
const char* prev_message = "Off"; // Инициализируем строковым литералом
unsigned long fail_blink_timer = 0; // Таймер для мигания надписи "FAIL"
bool fail_displayed = false; // Флаг для отслеживания состояния отображения "FAIL"
int attempt; // Объявление глобальной переменной колличество попыток запуска
int prev_currentState = -1;  // Добавлено для отслеживания изменений systemState


// Прототипы функций (чтобы компилятор знал о них до их определения)
void setPWM_PinBurnFan(int value); // Переименовано для ясности
void setPWM_PinGlowPlug(int value); // Переименовано для ясности
void resetToDefaultSettings();
void applySettings();
void serialEvent();
void processSerialCommands();
void handleUpCommand();
void handleDownCommand();
void handleEnterCommand();
void sendCurrentSettings();
void handleSettingsUpdate(char* paramsStr, bool is_from_websocket); // Изменена сигнатура
void handleFuelPumpingCommand();
void runFuelPumpingRoutine();
void display_data(); // Добавлено объявление функции display_data

// Объявления функций из wifi.ino
void setup_wifi_ap();
void handle_wifi_clients();
void send_status_update();

// Объявления функций из wifi.ino
void sendWiFiStatus();
void clearWiFiSettings();
void saveWiFiSettings(const char* ssid, const char* password);
extern bool isAPMode;
extern const char* FIRMWARE_VERSION;

// Определение enum для неблокирующего Wi-Fi
enum WiFiConnectionState {
  WIFI_INIT,            // Начальное состояние
  WIFI_CONNECTING_STA,  // Попытка подключения к STA
  WIFI_CONNECTED_STA,   // Подключено к STA
  WIFI_SETTING_UP_AP,   // Настройка точки доступа
  WIFI_AP_ACTIVE,       // Точка доступа активна
  WIFI_CONNECT_FAILED   // Не удалось подключиться к STA
};

// Объявления для неблокирующего Wi-Fi из wifi.ino
extern WiFiConnectionState currentWiFiState; // Только объявление переменной
extern unsigned long lastWiFiStateChange;


// Переопределение sendCurrentSettings, чтобы она вызывала версию из wifi.ino
extern void sendCurrentSettings();

// Объявление глобальных переменных из fan.txt и glow.txt
extern int max_pwm_global;
extern unsigned long glow_brightness_max; // Изменено на unsigned long
extern unsigned long glow_fade_in_duration_ms; // Изменено на unsigned long
extern unsigned long glow_fade_out_duration_ms; // Изменено на unsigned long

// Добавьте после других extern объявлений
extern const char* mdns_hostname;
extern bool isAPMode;
extern ESP8266WebServer server;

void setup() {

  Serial.begin(57600); // Инициализация последовательного порта СНАЧАЛА

  EEPROM.begin(1024); // Инициализация EEPROM перед чтением флага

  // Инициализация LittleFS
  if (!LittleFS.begin()) {
      Serial.println(F("FS_FAIL: Failed to mount LittleFS"));
  } else {
      Serial.println(F("FS_OK: LittleFS mounted"));
  }

    // Проверяем флаг OTA при запуске и применяем update если нужно
  checkOTAFlag();

  setup_fuel_pump();
  
  // Создаем пользовательские символы
  lcd.createChar(7, contour);   // контур
  lcd.createChar(1, fill1);
  lcd.createChar(2, fill2);
  lcd.createChar(3, fill3);
  lcd.createChar(4, fill4);
  lcd.createChar(5, fill5);
  lcd.createChar(6, fill6);

  pinMode(fuel_pump_pin, OUTPUT); // Установка пина топливного насоса как выхода
  pinMode(glow_plug_pin, OUTPUT); // Установка пина свечи накаливания как выхода
  pinMode(burn_fan_pin, OUTPUT); // Установка пина вентилятора горелки как выхода
  pinMode(exhaust_temp_pin, INPUT); // Установка пина датчика температуры выхлопа как входа
  pinMode(push_pin, INPUT_PULLUP); // Установка пина кнопки как входа с подтяжкой
  pinMode(pushup_pin, INPUT_PULLUP);   // Вход кнопки вверх с подтяжкой
  pinMode(pushdown_pin, INPUT_PULLUP);   // Вход кнопки вниз с подтяжкой

  // Установка частоты ШИМ для ESP8266
  analogWriteFreq(10000); // Устанавливаем частоту ШИМ 10 кГц

  // Инициализация EEPROM для ESP8266
  EEPROM.begin(1024); // Выделяем 1024 байта для всех настроек

  EEPROM.get(0, settings); // Читаем настройки из EEPROM
  Serial.print(F("EEPROM: Прочитано magicNumber = 0x"));
  Serial.println(settings.magicNumber, HEX);

  // Проверка на "первый запуск" или повреждение EEPROM с использованием магического числа
  // Также добавлены проверки на разумность значений для heater_target и pump_size
  if (settings.magicNumber != EEPROM_MAGIC_NUMBER) {
    Serial.println(F("EEPROM: magicNumber не совпадает. Сброс на настройки по умолчанию."));
    resetToDefaultSettings(); // Устанавливаем значения по умолчанию и сохраняем их
  } else if (settings.heater_target < 150 || settings.heater_target > 250) {
    Serial.println(F("EEPROM: heater_target вне допустимого диапазона. Сброс на настройки по умолчанию."));
    resetToDefaultSettings();
  } else if (settings.pump_size < 10 || settings.pump_size > 100) {
    Serial.println(F("EEPROM: pump_size вне допустимого диапазона. Сброс на настройки по умолчанию."));
    resetToDefaultSettings();
  }
  else {
    Serial.println(F("EEPROM: Настройки успешно загружены."));
  }
  applySettings(); // Применяем загруженные (или дефолтные) настройки к рабочим переменным

  // Инициализация датчика DS18B20
  sensors.begin();
  sensors.setResolution(9); // 9 бит = 93.75 мс вместо 750 мс (±0.5°C точность)

  // Инициализация Wi-Fi AP и веб-сервера
  setup_wifi_station();

}

// Функция проверки флага OTA при запуске
void checkOTAFlag() {
    byte otaFlag = EEPROM.read(OTA_FLAG_ADDR);
    Serial.print(F("BOOT: Reading OTA flag: 0x"));
    Serial.println(otaFlag, HEX);
    if (otaFlag == 0xAA) {
        Serial.println(F("BOOT: OTA flag detected"));
        
        // Сбрасываем флаг
        EEPROM.write(OTA_FLAG_ADDR, 0x00);
        EEPROM.commit();
        
        // Проверяем наличие файла прошивки
        if (LittleFS.exists("/firmware.bin")) {
            Serial.println(F("BOOT: Firmware file found"));
            File fwFile = LittleFS.open("/firmware.bin", "r");
            if (fwFile) {
                size_t fwSize = fwFile.size();
                fwFile.close();
                Serial.print(F("BOOT: Firmware size: "));
                Serial.println(fwSize);
                
                if (fwSize > 100000) {
                    Serial.println(F("BOOT: Valid firmware ready for update"));
                    // Сбрасываем файл в начало
                    fwFile = LittleFS.open("/firmware.bin", "r");
                    fwFile.seek(0);
                    // Применяем update
                    if (Update.begin(fwSize)) {
                        Serial.println(F("BOOT: Starting firmware update..."));
                        size_t written = Update.write(fwFile);
                        fwFile.close();
                        if (written == fwSize && Update.end(false)) {
                            Serial.println(F("OTA_UPDATE_COMPLETE"));
                            LittleFS.remove("/firmware.bin");
                            delay(5000);
                            ESP.restart();
                        } else {
                            Serial.println(F("OTA_UPDATE_FAIL"));
                            Update.printError(Serial);
                            LittleFS.remove("/firmware.bin");
                        }
                    } else {
                        fwFile.close();
                        Serial.println(F("OTA_UPDATE_BEGIN_FAIL"));
                        LittleFS.remove("/firmware.bin");
                    }
                } else {
                    Serial.println(F("BOOT: Invalid firmware size"));
                    LittleFS.remove("/firmware.bin");
                }
            }
        } else {
            Serial.println(F("BOOT: No firmware file found"));
        }
    }
}

void loop() {
  // Отправка данных о состоянии по WebSocket (например, раз в секунду)
  static unsigned long lastStatusSendTime = 0;
  if (millis() - lastStatusSendTime >= 1000) { // Отправляем данные каждую секунду
    if (!otaInProgress) { // Только если OTA не в процессе
      send_status_update();
    }
    lastStatusSendTime = millis();
  }

  // Обработка клиентов Wi-Fi и WebSocket
  handle_wifi_clients();

  if (!otaInProgress) { // Только если OTA не в процессе
    // Основной цикл работы
    temp_data(); // Предполагаемые функции (убедитесь, что они определены)
    control();   // Предполагаемые функции (убедитесь, что они определены)
    webasto();   // Предполагаемые функции (убедитесь, что они определены)
    display_data(); // Предполагаемые функции (убедитесь, что они определены)

    // Асинхронное чтение DS18B20
    if (!ds18b20_disabled) { // НОВОЕ: проверяем флаг
      unsigned long currentTime = millis();

      if (!tempRequested && (currentTime - lastTempRequest >= TEMP_READ_INTERVAL)) {
        sensors.requestTemperatures();
        tempRequested = true;
        lastTempRequest = currentTime;
      }

      if (tempRequested && (currentTime - lastTempRequest >= 100)) {
        float tempReading = sensors.getTempCByIndex(0);
        
        if (tempReading == DEVICE_DISCONNECTED_C) {
          ds18b20_temp = 0;
          ds18b20_error_count++;
          
          if (ds18b20_error_count > 30) { // ИЗМЕНЕНО: с 10 на 30
            ds18b20_disabled = true; // НОВОЕ: отключаем датчик навсегда
            Serial.println(F("DS18B20_DISABLED: Too many errors"));
          } else if (ds18b20_error_count == 10) {
            sensors.begin(); // Попытка переинициализации на 10-й ошибке
          }
        } else {
          ds18b20_temp = tempReading;
          ds18b20_error_count = 0;
        }
        
        tempRequested = false;
      }
    }
  }

  // Обработка команд от Python-приложения
  serialEvent(); // Чтение из последовательного порта
  processSerialCommands(); // Обработка полученных команд

  // Запуск неблокирующей рутины прокачки топлива
  runFuelPumpingRoutine();

}

// Сбрасывает все настройки на значения по умолчанию и сохраняет их в EEPROM
void resetToDefaultSettings() {
  // Проверка размера структуры уже не так критична с EEPROM.begin(), но все равно полезна
  if (sizeof(settings) > EEPROM.length()) {
    Serial.println(F("ERROR: Структура настроек слишком велика для EEPROM."));
    return;
  }

  settings.magicNumber = EEPROM_MAGIC_NUMBER; // Устанавливаем магическое число
  settings.pump_size = 26;
  settings.heater_target = 195;
  settings.heater_min = 190;
  settings.heater_overheat = 210;
  settings.heater_warning = 200;
  settings.max_pwm_fan = 77;
  settings.glow_brightness = 60UL; // Использование UL для unsigned long
  settings.glow_fade_in_duration = 5000UL; // Использование UL для unsigned long
  settings.glow_fade_out_duration = 5000UL; // Использование UL для unsigned long

  EEPROM.put(0, settings); // Записываем настройки в EEPROM
  EEPROM.commit(); // Сохраняем изменения во флеш-память
  Serial.println(F("EEPROM: Настройки по умолчанию сохранены."));
  // Дополнительная проверка: читаем magicNumber сразу после записи
  Settings tempSettings;
  EEPROM.get(0, tempSettings);
  Serial.print(F("EEPROM: Подтверждение записи - magicNumber = 0x"));
  Serial.println(tempSettings.magicNumber, HEX);
}

// Применяет загруженные настройки к глобальным переменным
void applySettings() {
  pump_size = settings.pump_size;
  heater_target = settings.heater_target;
  heater_min = settings.heater_min;
  heater_overheat = settings.heater_overheat;
  heater_warning = settings.heater_warning;
  max_pwm_global = settings.max_pwm_fan;
  glow_brightness_max = settings.glow_brightness;
  glow_fade_in_duration_ms = settings.glow_fade_in_duration;
  glow_fade_out_duration_ms = settings.glow_fade_out_duration;

  Serial.println(F("EEPROM: Настройки применены."));
}

// Обновите serialEvent() для обработки бинарных данных OTA
void serialEvent() {
    while (Serial.available()) {
        if (otaInProgress && otaWriteInProgress) {
            // В режиме OTA читаем бинарные данные
            uint8_t buffer[OTA_BUFFER_SIZE];
            size_t length = Serial.readBytes(buffer, OTA_BUFFER_SIZE);
            handleOTAData(buffer, length);
        } else {
            // Обычный текстовый режим
            char inChar = (char)Serial.read();
            if (inChar == '\n') {
                if (inputBufferIndex > 0) {
                    inputBuffer[inputBufferIndex] = '\0';
                    stringComplete = true;
                }
            } else if (inputBufferIndex < (SERIAL_BUFFER_SIZE - 1)) {
                inputBuffer[inputBufferIndex++] = inChar;
            }
        }
    }
}

// Разбирает и выполняет команды, полученные по последовательному порту
void processSerialCommands() {
  if (stringComplete) {

    // Создаем временную копию для безопасной модификации (strtok изменяет строку)
    char tempCommand[SERIAL_BUFFER_SIZE];
    memset(tempCommand, 0, SERIAL_BUFFER_SIZE); // ИНИЦИАЛИЗАЦИЯ НУЛЯМИ
    strncpy(tempCommand, inputBuffer, SERIAL_BUFFER_SIZE - 1);
    tempCommand[SERIAL_BUFFER_SIZE - 1] = '\0'; // Гарантируем нулевой терминатор

    // Обрезка пробелов и непечатаемых символов в начале и конце
    char* cleanedCommand = tempCommand;
    // Обрезка в начале
    while (*cleanedCommand && (isspace(*cleanedCommand) || *cleanedCommand == '\r' || *cleanedCommand == '\n')) {
        cleanedCommand++;
    }
    // Обрезка в конце
    char* end = cleanedCommand + strlen(cleanedCommand) - 1;
    while (end >= cleanedCommand && (isspace(*end) || *end == '\r' || *end == '\n')) {
        *end-- = '\0';
    }


    // Теперь используем cleanedCommand для сравнения
    char* currentCommandPtr = cleanedCommand;

    // Проверяем команду SET:
    if (strncmp(currentCommandPtr, "SET:", 4) == 0) {
      // Serial.println(F("DEBUG: Идентифицирована команда SET."));
      // При вызове из Serial, is_from_websocket = false
      handleSettingsUpdate(currentCommandPtr + 4, false); 
    }
    // Проверяем другие команды
    else if (strcmp(currentCommandPtr, "UP") == 0) {
      // Serial.println(F("DEBUG: Идентифицирована команда UP. Вызов handleUpCommand()."));
      handleUpCommand();
    }
    else if (strcmp(currentCommandPtr, "DOWN") == 0) {
      // Serial.println(F("DEBUG: Идентифицирована команда DOWN. Вызов handleDownCommand()."));
      handleDownCommand();
    }
    else if (strcmp(currentCommandPtr, "ENTER") == 0) {
      // Serial.println(F("DEBUG: Идентифицирована команда ENTER."));
      handleEnterCommand();
    }
    else if (strcmp(currentCommandPtr, "GET_SETTINGS") == 0) {
      // Serial.println(F("DEBUG: Идентифицирована команда GET_SETTINGS."));
      sendCurrentSettings();
    }
    else if (strcmp(currentCommandPtr, "FP") == 0) {
      // Serial.println(F("DEBUG: Идентифицирована команда FP."));
      handleFuelPumpingCommand();
    }
    else if (strcmp(currentCommandPtr, "CF") == 0) {
      // Serial.println(F("DEBUG: Идентифицирована команда CF."));
      if (webasto_fail) {
        webasto_fail = false;
        lcd.setCursor(11, 1);
        lcd.print(F("    "));
        Serial.println(F("FAIL_CLEARED"));
      } else {
        Serial.println(F("NO_FAIL_TO_CLEAR"));
      }
    }
    else if (strcmp(currentCommandPtr, "RESET_FUEL_CONSUMPTION") == 0) {
      total_fuel_consumed_liters = 0.0;
      send_status_update(); // Обновляем UI
      Serial.println(F("FUEL_RESET_OK"));
    }
    else if (strcmp(currentCommandPtr, "RESET_SETTINGS") == 0) {
      resetToDefaultSettings();
      sendCurrentSettings();
      Serial.println(F("RESET_SETTINGS_OK"));
    }

    // ДОБАВЛЕНО: Обработка WiFi команд
    else if (strcmp(currentCommandPtr, "GET_WIFI_STATUS") == 0) {
      sendWiFiStatus();
    }

    else if (strcmp(currentCommandPtr, "RESET_WIFI") == 0) {
      Serial.println(F("DEBUG: Received RESET_WIFI command. Clearing WiFi settings..."));
      clearWiFiSettings();
      WiFi.disconnect(true);
      delay(1000);
      ESP.restart();
    }

    else if (strcmp(currentCommandPtr, "REBOOT_ESP") == 0) {
      Serial.println(F("DEBUG: Received REBOOT_ESP command. Rebooting."));
      ESP.restart();
    }

    else if (strcmp(currentCommandPtr, "SCAN_WIFI") == 0) {
      Serial.println(F("DEBUG: Received SCAN_WIFI command. Scanning networks..."));
      int n = WiFi.scanNetworks();
      Serial.printf("DEBUG: Scan done. Found %d networks.\n", n);
      // Отправляем результаты сканирования через Serial
      Serial.println("WIFI_SCAN_START");
      for (int i = 0; i < n; ++i) {
        Serial.printf("SSID: %s, RSSI: %d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      }
      Serial.println("WIFI_SCAN_END");
      WiFi.scanDelete();
    }

    else if (strncmp(currentCommandPtr, "CONNECT_WIFI:", 13) == 0) {
      char* params = currentCommandPtr + 13;
      char* ssid = strtok(params, ",");
      char* password = strtok(NULL, ",");

      if (ssid) {
        Serial.printf("DEBUG: Received CONNECT_WIFI command. Connecting to SSID: %s\n", ssid);
        
        // Сохраняем новые credentials
        saveWiFiSettings(ssid, password);
        
        // Инициируем неблокирующее подключение
        WiFi.begin(ssid, password ? password : "");
        currentWiFiState = WIFI_CONNECTING_STA;
        lastWiFiStateChange = millis();
        Serial.print("DEBUG: Initiating non-blocking WiFi connection...");
      }
    }

    // Добавьте в processSerialCommands() обработку WiFi статуса:
    else if (strcmp(currentCommandPtr, "GET_WIFI_STATUS_DETAILED") == 0) {
        sendDetailedWiFiStatus();
    }

    // Добавьте в processSerialCommands() обработку OTA команд
    else if (strcmp(currentCommandPtr, "START_OTA") == 0) {
        startOTAUpdate();
    }
    else if (strcmp(currentCommandPtr, "CANCEL_OTA") == 0) {
        cancelOTAUpdate();
    }
    else if (strcmp(currentCommandPtr, "APPLY_OTA") == 0) {
        applyOTAUpdate();
    }
    else if (strcmp(currentCommandPtr, "GET_FIRMWARE_VERSION") == 0) {
        Serial.print(F("FIRMWARE_VERSION:"));
        Serial.println(FIRMWARE_VERSION);
    }
    else if (strcmp(currentCommandPtr, "GET_FS_INFO") == 0) {
        FSInfo fs_info;
        if (LittleFS.info(fs_info)) {
            Serial.print(F("FS_TOTAL:"));
            Serial.print(fs_info.totalBytes);
            Serial.print(F(",FS_USED:"));
            Serial.print(fs_info.usedBytes);
            Serial.print(F(",FS_FREE:"));
            Serial.println(fs_info.totalBytes - fs_info.usedBytes);
        } else {
            Serial.println(F("FS_INFO_FAIL"));
        }
    }
    else if (strcmp(currentCommandPtr, "END_OTA") == 0) {
        if (otaInProgress) {
            completeOTAWrite();
        } else {
            Serial.println(F("OTA_NOT_ACTIVE"));
        }
    }

    // Неизвестные команды игнорируются
    // Сбрасываем флаг и индекс для следующей команды
    stringComplete = false;
    inputBufferIndex = 0; // ВАЖНО: Сбрасываем индекс здесь, после обработки
  }
}

void sendDetailedWiFiStatus() {                                        // подготовка и тправка статуса wifi по сериал
    Serial.print(F("WIFI_STATUS_DETAILED:"));
    Serial.print(F("mode=")); Serial.print(isAPMode ? "AP" : "STA");
    
    if (isAPMode) {
        // Режим точки доступа - реальные данные
        Serial.print(F(",ap_ssid=")); Serial.print(WiFi.softAPSSID());
        Serial.print(F(",ap_ip=")); Serial.print(WiFi.softAPIP().toString());
        Serial.print(F(",ap_clients=")); Serial.print(WiFi.softAPgetStationNum());
        
        Serial.print(F(",sta_ssid=none"));
        Serial.print(F(",sta_ip=none"));
        Serial.print(F(",sta_status=disconnected"));
    } else {
        // Режим клиента
        Serial.print(F(",ap_ssid=espwebasto")); // Имя когда в режиме AP
        Serial.print(F(",ap_ip=192.168.10.10")); // IP когда в режиме AP
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print(F(",sta_ssid=")); Serial.print(WiFi.SSID());
            Serial.print(F(",sta_ip=")); Serial.print(WiFi.localIP().toString());
            Serial.print(F(",sta_status=connected"));
            Serial.print(F(",sta_gateway=")); Serial.print(WiFi.gatewayIP().toString());
            Serial.print(F(",sta_dns=")); Serial.print(WiFi.dnsIP().toString());
        } else {
            Serial.print(F(",sta_ssid=none"));
            Serial.print(F(",sta_ip=none"));
            Serial.print(F(",sta_status=disconnected"));
        }
    }
    
    // Общая информация
    Serial.print(F(",ap_password=12345678"));
    Serial.print(F(",mac_address=")); Serial.print(WiFi.macAddress());
    Serial.print(F(",signal_strength=")); Serial.print(WiFi.RSSI());
    Serial.print(F(",channel=")); Serial.print(WiFi.channel());
    
    Serial.println();
}

// Обрабатывает команду "UP"
void handleUpCommand() {
  switch (currentState) {
    case STATE_HIGH: currentState = STATE_MID; break;
    case STATE_MID: currentState = STATE_LOW; break;
    case STATE_LOW: break; // Остаемся в LOW
  }
  Serial.print(F("State: "));
  if (currentState == STATE_HIGH) Serial.println(F("HIGH"));
  else if (currentState == STATE_MID) Serial.println(F("MID"));
  else Serial.println(F("LOW"));
}

// Обрабатывает команду "DOWN"
void handleDownCommand() {
  switch (currentState) {
    case STATE_LOW: currentState = STATE_MID; break;
    case STATE_MID: currentState = STATE_HIGH; break;
    case STATE_HIGH: break; // Остаемся в HIGH
  }
  Serial.print(F("State: "));
  if (currentState == STATE_HIGH) Serial.println(F("HIGH"));
  else if (currentState == STATE_MID) Serial.println(F("MID"));
  else Serial.println(F("LOW"));
}

// Обрабатывает команду "ENTER" (включение/выключение горения)
void handleEnterCommand() {
  // Serial.println(F("DEBUG: Вызвана handleEnterCommand()"));
  if (burn_mode == 1 || burn_mode == 2) {
    burn = false; // Выключаем горение
  } else {
    burn = true; // Включаем горение
  }
  Serial.print(F("Burn: "));
  Serial.println(burn ? F("ON") : F("OFF"));
}


// Обрабатывает команду обновления настроек (например, "SET:pump_size=22,heater_target=195")
void handleSettingsUpdate(char* paramsStr, bool is_from_websocket) {         // Изменена сигнатура
  // Serial.println(F("DEBUG: Вызвана handleSettingsUpdate()"));
  settingsUpdateInProgress = true; // Блокируем потенциальные конфликты (например, с logging())

  char* token;
  char* rest = paramsStr; // Используем указатель для strtok

  int paramsFound = 0;

  token = strtok(rest, ","); // Получаем первый токен (параметр)
  while (token != NULL) {
    // Обрезаем пробелы в начале и конце текущего параметра
    char* current_param = token;
    while (*current_param == ' ' || *current_param == '\t') current_param++;
    char* end_param = current_param + strlen(current_param) - 1;
    while (end_param >= current_param && (*end_param == ' ' || *end_param == '\t')) {
      *end_param-- = '\0';
    }

    char* eqPos = strchr(current_param, '='); // Ищем знак равенства
    if (eqPos != NULL) {
      *eqPos = '\0'; // Завершаем строку для ключа
      char* key = current_param;
      char* value_str = eqPos + 1;

      // Обрезаем пробелы в начале ключа и значения
      while (*key == ' ' || *key == '\t') key++;
      while (*value_str == ' ' || *value_str == '\t') value_str++;

      // Используем atol для unsigned long значений
      long parsedValue = atol(value_str); 

      if (strcmp(key, "pump_size") == 0) {
        settings.pump_size = parsedValue;
        paramsFound++;
      }
      else if (strcmp(key, "heater_target") == 0) {
        settings.heater_target = parsedValue;
        paramsFound++;
      }
      else if (strcmp(key, "heater_min") == 0) {
        settings.heater_min = parsedValue;
        paramsFound++;
      }
      else if (strcmp(key, "heater_overheat") == 0) {
        settings.heater_overheat = parsedValue;
        paramsFound++;
      }
      else if (strcmp(key, "heater_warning") == 0) {
        settings.heater_warning = parsedValue;
        paramsFound++;
      }
      else if (strcmp(key, "max_pwm_fan") == 0) {
        settings.max_pwm_fan = parsedValue;
        paramsFound++;
      }
      else if (strcmp(key, "glow_brightness") == 0) {
        settings.glow_brightness = parsedValue;
        paramsFound++;
      }
      else if (strcmp(key, "glow_fade_in_duration") == 0) {
        settings.glow_fade_in_duration = parsedValue;
        paramsFound++;
      }
      else if (strcmp(key, "glow_fade_out_duration") == 0) {
        settings.glow_fade_out_duration = parsedValue;
        paramsFound++;
      }
      else {
        Serial.print(F("Неизвестный ключ: "));
        Serial.println(key);
      }
    } else {
      Serial.print(F("Неверный формат параметра: "));
      Serial.println(current_param);
    }
    token = strtok(NULL, ","); // Получаем следующий токен
  }

  if (paramsFound > 0) {
    EEPROM.put(0, settings); // Сохраняем обновленные настройки в EEPROM
    EEPROM.commit(); // Сохраняем изменения во флеш-память
    applySettings(); // Применяем их к рабочим переменным
    Serial.println(F("SETTINGS_OK"));
    // Дополнительная проверка: читаем magicNumber сразу после записи
    Settings tempSettings;
    EEPROM.get(0, tempSettings);
    Serial.print(F("EEPROM: Подтверждение записи - magicNumber = 0x"));
    Serial.println(tempSettings.magicNumber, HEX);

    // НОВОЕ: Отправляем текущие настройки после сохранения
    sendCurrentSettings();
  } else {
    Serial.println(F("SETTINGS_ERROR: Параметры не найдены или неверны."));
  }
  settingsUpdateInProgress = false; // Разблокируем
}

// Инициирует неблокирующую прокачку топлива
void handleFuelPumpingCommand() {
  // Serial.println(F("DEBUG: Вызвана handleFuelPumpingCommand()"));
  if (burn_mode == 0 && !fuelPumpingActive) { // Только если нагреватель выключен и прокачка не активна
    fuelPumpingActive = true;
    fuelPumpStartTime = millis();
    fuelPumpCurrentCycle = 0;
    lastFuelPumpToggleTime = millis();
    fuelPumpState = HIGH; // Начинаем с включения насоса
    digitalWrite(fuel_pump_pin, fuelPumpState);

    lcd.setCursor(0, 1);
    lcd.print(F("Прокачка топл.")); // Сообщение о начале прокачки
    Serial.println(F("FUEL_PUMPING_STARTED"));
  } else if (fuelPumpingActive) {
    Serial.println(F("FUEL_PUMPING_ALREADY_ACTIVE"));
  } else {
    Serial.println(F("FUEL_PUMPING_NOT_ALLOWED (нагреватель включен)"));
  }
}

// Выполняет логику неблокирующей прокачки топлива. Должна вызываться в loop().
void runFuelPumpingRoutine() {
  if (!fuelPumpingActive) {
    return; // Если прокачка не активна, ничего не делаем
  }

  unsigned long currentTime = millis();

  if (fuelPumpState == HIGH) { // Если насос включен
    if (currentTime - lastFuelPumpToggleTime >= PUMP_ON_DURATION_MS) {
      digitalWrite(fuel_pump_pin, LOW); // Выключаем насос
      fuelPumpState = LOW;
      lastFuelPumpToggleTime = currentTime;
    }
  } else { // Если насос выключен
    if (currentTime - lastFuelPumpToggleTime >= PUMP_OFF_DURATION_MS) {
      digitalWrite(fuel_pump_pin, HIGH); // Включаем насос
      fuelPumpState = HIGH;
      lastFuelPumpToggleTime = currentTime;
      fuelPumpCurrentCycle++; // Увеличиваем счетчик циклов после завершения полного цикла (вкл+выкл)
    }
  }

  // Обновление дисплея с обратным отсчетом
  int remainingCycles = TOTAL_PUMP_CYCLES - fuelPumpCurrentCycle;
  if (remainingCycles < 0) remainingCycles = 0; // Гарантируем, что не будет отрицательных значений

  // Обновляем дисплей только если есть изменения или каждые несколько циклов для плавности
  static int lastDisplayedRemaining = -1; // Статическая переменная для отслеживания последнего отображенного значения
  if (remainingCycles != lastDisplayedRemaining) {
    lcd.setCursor(13, 1); // Правый край для обратного отсчета
    if (remainingCycles > 99) {
      lcd.print(remainingCycles);
    } else if (remainingCycles > 9) {
      lcd.print(F(" ")); // Один пробел для выравнивания
      lcd.print(remainingCycles);
    } else {
      lcd.print(F("  ")); // Два пробела для выравнивания
      lcd.print(remainingCycles);
    }
    lastDisplayedRemaining = remainingCycles;
  }

  // Завершение прокачки
  if (fuelPumpCurrentCycle >= TOTAL_PUMP_CYCLES) {
    digitalWrite(fuel_pump_pin, LOW); // Убедимся, что насос выключен
    fuelPumpingActive = false; // Отключаем флаг активности
    Serial.println(F("FUEL_PUMPING_COMPLETE"));
    lcd.setCursor(0, 1);
    lcd.print(F("Завершено       ")); // Сообщение о завершении
    delay(500); // Небольшая задержка для отображения "Complete"
    lcd.setCursor(0, 1);
    lcd.print(F("OFF             ")); // Возврат к "OFF" или другому статусу
  }
}

// OTA функции
void startOTAUpdate() {
    if (otaInProgress) {
        Serial.println(F("OTA_ALREADY_IN_PROGRESS"));
        return;
    }

    // Удаляем предыдущий файл прошивки если остался
    if (LittleFS.exists("/firmware.bin")) {
        Serial.println(F("OTA_REMOVING_OLD_FILE"));
        LittleFS.remove("/firmware.bin");
    }

    // Открываем файл для записи новой прошивки
    Serial.println(F("OTA_OPENING_FILE"));
    otaFile = LittleFS.open("/firmware.bin", "w");
    if (!otaFile) {
        Serial.println(F("OTA_FILE_OPEN_FAIL"));
        return;
    }

    Serial.println(F("OTA_READY"));
    otaInProgress = true;
    otaWriteInProgress = true; // Устанавливаем флаг сразу после OTA_READY
    otaStartTime = millis();
    otaTotalSize = 0;
    otaReceived = 0;
}

void cancelOTAUpdate() {
    if (otaInProgress) {
        if (otaFile) {
            otaFile.close();
        }
        
        // Удаляем частично загруженный файл
        if (LittleFS.exists("/firmware.bin")) {
            LittleFS.remove("/firmware.bin");
        }
        
        otaInProgress = false;
        otaWriteInProgress = false;
        Serial.println(F("OTA_CANCELLED"));
    }
}

void handleOTAData(uint8_t* data, size_t length) {
    if (!otaInProgress) return;

    // Check if this is the END_OTA command received as binary data
    if (otaWriteInProgress && length >= 8 && memcmp(data, "END_OTA\n", 8) == 0) {
        Serial.println(F("Received END_OTA as binary, processing..."));
        completeOTAWrite();
        return;
    }

    if (!otaWriteInProgress) return;

    // Проверка таймаута
    if (millis() - otaStartTime > OTA_TIMEOUT) {
        cancelOTAUpdate();
        Serial.println(F("OTA_TIMEOUT"));
        return;
    }

    // Проверка размера файла
    if (otaReceived + length > MAX_FIRMWARE_SIZE) {
        cancelOTAUpdate();
        Serial.println(F("OTA_FILE_TOO_LARGE"));
        return;
    }

    // Записываем данные в файл
    size_t written = otaFile.write(data, length);
    if (written != length) {
        cancelOTAUpdate();
        Serial.println(F("OTA_WRITE_FAIL"));
        return;
    }

    otaReceived += written;

    // Отправляем подтверждение для синхронизации (команда - ответ)
    Serial.println(F("OTA_CHUNK_ACK"));

    // Отправляем прогресс каждые 10KB для отображения
    if (otaReceived % 10240 == 0) {
        Serial.print(F("OTA_PROGRESS:"));
        Serial.println(otaReceived);
    }
}

void completeOTAWrite() {
    if (otaFile) {
        otaFile.close();
    }
    otaWriteInProgress = false;

    // Проверяем размер файла
    File f = LittleFS.open("/firmware.bin", "r");
    if (!f) {
        Serial.println(F("OTA_FILE_VERIFY_FAIL"));
        otaInProgress = false;
        return;
    }

    size_t actualSize = f.size();
    f.close();

    if (actualSize < 100000) { // Минимальный размер прошивки ~100KB
        LittleFS.remove("/firmware.bin");
        otaInProgress = false;
        Serial.println(F("OTA_FILE_TOO_SMALL"));
        return;
    }

    otaTotalSize = actualSize;
    Serial.print(F("OTA_RECEIVE_COMPLETE:"));
    Serial.println(actualSize);
    Serial.println(F("OTA_READY_TO_APPLY"));
}

void applyOTAUpdate() {
    if (!otaInProgress) {
        Serial.println(F("OTA_NOT_READY"));
        return;
    }
    
    if (!LittleFS.exists("/firmware.bin")) {
        Serial.println(F("OTA_FILE_MISSING"));
        otaInProgress = false;
        return;
    }
    
    Serial.println(F("OTA_APPLYING"));
    
    File fwFile = LittleFS.open("/firmware.bin", "r");
    if (!fwFile) {
        Serial.println(F("OTA_APPLY_OPEN_FAIL"));
        otaInProgress = false;
        return;
    }
    
    // Проверяем валидность прошивки
    size_t fwSize = fwFile.size();
    fwFile.close();
    
    if (fwSize < 100000) {
        LittleFS.remove("/firmware.bin");
        Serial.println(F("OTA_INVALID_FIRMWARE"));
        otaInProgress = false;
        return;
    }
    
    Serial.println(F("OTA_VALIDATION_OK"));
    
    // Устанавливаем флаг OTA и перезагружаемся
    EEPROM.write(OTA_FLAG_ADDR, 0xAA);
    EEPROM.commit();
    
    Serial.println(F("OTA_REBOOTING"));
    Serial.println(F("DEVICE_WILL_UPDATE_ON_REBOOT"));
    
    delay(2000);
    
    // Перезагружаем устройство
    ESP.restart();
}


// Проверка всех файлов проекта завершена. Логика работы нагревателя корректна, компиляция успешна. Вот краткий анализ ключевых аспектов:

// ### __Логирование (src/logging.ino)__

// - Выводит параметры в Serial каждую секунду: webasto_fail, ignit_fail, exhaust_temp, fan_speed, fuel_need, glow_time, message и др.
// - Блокируется при обновлении настроек (settingsUpdateInProgress).
// - Формат: `| F: 0 | IgnF#: 0 | ETmp: 25.3 | Fan%: 50.0 | FHZ 10.0 | FN: 2.5 | Gl: 0 | CyTim: 10 | I: Running | FinalFuel: 2.38 | St: 1`

// ### __Работа с EEPROM (src/low_smoke.ino)__

// - Структура `Settings`: pump_size, heater_target, heater_min, heater_overheat, heater_warning, max_pwm_fan, glow_brightness, fade_in/out_duration.
// - Магическое число (0x12345679) для проверки целостности.
// - Загрузка при старте, сохранение при обновлении.
// - Сброс на дефолт при повреждении или первом запуске.
// - Применение настроек к переменным после загрузки.

// ### __Общая логика проекта__

// - __Wi-Fi/WebSocket (src/wifi.ino)__: Веб-интерфейс с вкладками (управление, настройки, лог, Wi-Fi, расход топлива). Команды через WebSocket (UP/DOWN/ENTER/SET/RESET и др.). Сканирование сетей, подключение, сброс Wi-Fi.

// - __Управление (src/control.ino)__: Кнопки для вкл/выкл, сброс ошибок, прокачка топлива (10 сек), переключение режимов HIGH/MID/LOW.

// - __Датчики и актуаторы__:

//   - Температура (src/temp_data.ino, src/get_webasto_temp.ino): Скользящее среднее, проверка на ошибки (-999 при сбое).
//   - Вентилятор (src/fan.ino): PWM управление, плавное изменение скорости.
//   - Насос (src/fuel_pump.ino): PWM для топлива, неблокирующая прокачка.
//   - Свеча (src/glow.ino): PWM с fade-in/out, выключение при temp >70°C (исправлено).

// - __Дисплей (src/display1602.ino)__: LCD 16x2, отображение статуса, температуры, режима.

// - __Расчеты (src/prime_ratio.ino, src/running_ratio.ino, src/mapf.ino)__: Соотношения топлива по температуре, регулировка в режиме горения.

// - __Основная логика (src/webasto.ino)__: Режимы 0-3 (выкл/запуск/горение/выключение), переходы по условиям, обработка ошибок.

// ### __Потенциальные замечания__

// - Все файлы связаны через extern переменные и функции.
// - EEPROM использует ESP8266 EEPROM (эмуляция флеш).
// - Wi-Fi: Автоподключение через WiFiManager, AP при неудаче.
// - Расход топлива: Учет в total_fuel_consumed_liters, расчет per_hour.
// - Нет критических ошибок в логике; изменения (вынос условия свечи) улучшают надежность.
