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

// Укажите адрес вашего I2C дисплея (обычно 0x27 или 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Установите адрес, количество символов и строк

// Переименовываем состояния, чтобы избежать конфликта с Arduino константами
enum SystemState {
  STATE_HIGH,   // вместо HIGH        // 0
  STATE_MID,    // вместо MID         // 1
  STATE_LOW     // вместо LOW         // 2
};
SystemState currentState = STATE_HIGH;  // Начальное состояние

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

// Буфер для входящих команд
const int SERIAL_BUFFER_SIZE = 128; // Увеличен буфер для длинных команд SET
char inputBuffer[SERIAL_BUFFER_SIZE];
byte inputBufferIndex = 0;
bool stringComplete = false;

// Подключения пинов для NodeMCU v2 ESP12E
// Используем D-пины, которые соответствуют GPIO на ESP8266
int fuel_pump_pin = D5; // GPIO14
int glow_plug_pin = D6; // GPIO12
int burn_fan_pin = D7; // GPIO13
int exhaust_temp_pin = A0; // ADC0, единственный аналоговый вход на NodeMCU
int push_pin = D1; // GPIO5
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
float start_fan_speed = 20; // Начальная скорость вентилятора
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

const unsigned long PUMP_ON_DURATION_MS = 30;  // Длительность включения насоса в мс
const unsigned long PUMP_OFF_DURATION_MS = 110; // Длительность выключения насоса в мс
const int TOTAL_PUMP_CYCLES = 200; // Общее количество циклов прокачки

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
// Переопределение sendCurrentSettings, чтобы она вызывала версию из wifi.ino
extern void sendCurrentSettings();

// Объявление глобальных переменных из fan.txt и glow.txt
extern int max_pwm_global;
extern unsigned long glow_brightness_max; // Изменено на unsigned long
extern unsigned long glow_fade_in_duration_ms; // Изменено на unsigned long
extern unsigned long glow_fade_out_duration_ms; // Изменено на unsigned long


void setup() {
  lcd.init(); // Инициализация дисплея
  lcd.backlight(); // Включение подсветки



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
  analogWriteFreq(500); // Устанавливаем частоту ШИМ 500 Гц

  Serial.begin(9600); // Инициализация последовательного порта

  // Инициализация EEPROM для ESP8266
  EEPROM.begin(sizeof(Settings)); // Выделяем необходимое количество байт для структуры настроек

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
  
    // Инициализация Wi-Fi AP и веб-сервера
  setup_wifi_station();
}

void loop() {
  // Отправка данных о состоянии по WebSocket (например, раз в секунду)
  static unsigned long lastStatusSendTime = 0;
  if (millis() - lastStatusSendTime >= 1000) { // Отправляем данные каждую секунду
    send_status_update();
    lastStatusSendTime = millis();
  }


  // Обработка клиентов Wi-Fi и WebSocket
  handle_wifi_clients();

  // Основной цикл работы
  temp_data(); // Предполагаемые функции (убедитесь, что они определены)
  control();   // Предполагаемые функции (убедитесь, что они определены)
  webasto();   // Предполагаемые функции (убедитесь, что они определены)
  display_data(); // Предполагаемые функции (убедитесь, что они определены)


  // Обработка команд от Python-приложения
  serialEvent(); // Чтение из последовательного порта
  processSerialCommands(); // Обработка полученных команд

  // Запуск неблокирующей рутины прокачки топлива
  runFuelPumpingRoutine();

  // Отладка текущего состояния (закомментировано)
  /*
  static SystemState lastPrintedState = (SystemState)-1; // Инициализируем чем-то отличным от реальных состояний
  if (currentState != lastPrintedState) {
      Serial.print(F("Current State: "));
      if (currentState == STATE_HIGH) Serial.println(F("HIGH"));
      else if (currentState == STATE_MID) Serial.println(F("MID"));
      else if (currentState == STATE_LOW) Serial.println(F("LOW"));
      lastPrintedState = currentState;
  }
  // Отладка температуры и режима горения (закомментировано)
  static unsigned long debug_print_timer = 0;
  if (millis() - debug_print_timer >= 1000) { // Каждую секунду
      Serial.print(F(" exhaust_temp: ")); Serial.print(exhaust_temp);
      Serial.print(F(", burn: ")); Serial.println(burn ? F("ON") : F("OFF"));
      debug_print_timer = millis();
  }
  */
}

// Сбрасывает все настройки на значения по умолчанию и сохраняет их в EEPROM
void resetToDefaultSettings() {
  // Проверка размера структуры уже не так критична с EEPROM.begin(), но все равно полезна
  if (sizeof(settings) > EEPROM.length()) {
    Serial.println(F("ERROR: Структура настроек слишком велика для EEPROM."));
    return;
  }

  settings.magicNumber = EEPROM_MAGIC_NUMBER; // Устанавливаем магическое число
  settings.pump_size = 22;
  settings.heater_target = 195;
  settings.heater_min = 190;
  settings.heater_overheat = 210;
  settings.heater_warning = 200;
  // Установка значений по умолчанию для новых переменных
  settings.max_pwm_fan = 255 * 0.75;
  settings.glow_brightness = 115UL; // Использование UL для unsigned long
  settings.glow_fade_in_duration = 10000UL; // Использование UL для unsigned long
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
  // Применение новых переменных
  max_pwm_global = settings.max_pwm_fan;
  glow_brightness_max = settings.glow_brightness;
  glow_fade_in_duration_ms = settings.glow_fade_in_duration;
  glow_fade_out_duration_ms = settings.glow_fade_out_duration;

  Serial.println(F("EEPROM: Настройки применены."));
}

// Обрабатывает входящие данные из последовательного порта
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') { // Обрабатываем и \n как конец строки
      if (inputBufferIndex > 0) { // Обрабатываем только если есть данные
         inputBuffer[inputBufferIndex] = '\0'; // Добавляем нулевой терминатор
         stringComplete = true;
      }
    } else if (inputBufferIndex < (SERIAL_BUFFER_SIZE - 1)) {
      inputBuffer[inputBufferIndex++] = inChar;
    }
  }
}

// Разбирает и выполняет команды, полученные по последовательному порту
void processSerialCommands() {
  if (stringComplete) {
    /*
    Serial.print(F("DEBUG: Получена команда (сырой буфер): '"));
    Serial.print(inputBuffer); // Выводим буфер до обрезки
    Serial.print(F("', длина: "));
    Serial.println(strlen(inputBuffer));
    */

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

    /*
    Serial.print(F("DEBUG: Обработанная команда: '"));
    Serial.print(cleanedCommand);
    Serial.print(F("', длина: "));
    Serial.println(strlen(cleanedCommand));
    */

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
    else {
      /*
      Serial.print(F("DEBUG: Неизвестная команда или несоответствие: '"));
      Serial.print(currentCommandPtr);
      Serial.println(F("'"));
      // Дополнительная отладка для неизвестных команд
      for (int i = 0; i < strlen(cleanedCommand); i++) { // Итерируем по обрезанной строке
        Serial.print(F(" Char[")); Serial.print(i); Serial.print(F("]: '"));
        Serial.print(cleanedCommand[i]); Serial.print(F("' ("));
        Serial.print((int)cleanedCommand[i]); Serial.println(F(")"));
      }
      */
    }
    // Сбрасываем флаг и индекс для следующей команды
    stringComplete = false;
    inputBufferIndex = 0; // ВАЖНО: Сбрасываем индекс здесь, после обработки
  }
}

// Обрабатывает команду "UP"
void handleUpCommand() {
  // Serial.println(F("DEBUG: Вызвана handleUpCommand()"));
  if (exhaust_temp > 150 || burn == 0) {
    switch (currentState) {
      case STATE_HIGH: currentState = STATE_MID; break;
      case STATE_MID: currentState = STATE_LOW; break;
      case STATE_LOW: break; // Остаемся в LOW
    }
  } else {
    // Serial.println(F("DEBUG: Условие для UP не выполнено (exhaust_temp <= 150 и burn == 1)"));
  }
  Serial.print(F("State: "));
  if (currentState == STATE_HIGH) Serial.println(F("HIGH"));
  else if (currentState == STATE_MID) Serial.println(F("MID"));
  else Serial.println(F("LOW"));
}

// Обрабатывает команду "DOWN"
void handleDownCommand() {
  // Serial.println(F("DEBUG: Вызвана handleDownCommand()"));
  if (exhaust_temp > 150 || burn == 0) {
    switch (currentState) {
      case STATE_LOW: currentState = STATE_MID; break;
      case STATE_MID: currentState = STATE_HIGH; break;
      case STATE_HIGH: break; // Остаемся в HIGH
    }
  } else {
    // Serial.println(F("DEBUG: Условие для DOWN не выполнено (exhaust_temp <= 150 и burn == 1)"));
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

// // Отправляет текущие настройки по последовательному порту
// void sendCurrentSettings() {
//   Serial.print(F("CURRENT_SETTINGS:"));
//   Serial.print(F("pump_size=")); Serial.print(settings.pump_size); Serial.print(F(","));
//   Serial.print(F("heater_target=")); Serial.print(settings.heater_target); Serial.print(F(","));
//   Serial.print(F("heater_min=")); Serial.print(settings.heater_min); Serial.print(F(","));
//   Serial.print(F("heater_overheat=")); Serial.print(settings.heater_overheat); Serial.print(F(","));
//   Serial.print(F("heater_warning=")); Serial.println(settings.heater_warning);
// }

// Обрабатывает команду обновления настроек (например, "SET:pump_size=22,heater_target=195")
void handleSettingsUpdate(char* paramsStr, bool is_from_websocket) { // Изменена сигнатура
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
      // Эти параметры можно изменять только через WebSocket
      else if (is_from_websocket) {
        if (strcmp(key, "max_pwm_fan") == 0) {
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
      }
      else {
        Serial.print(F("Настройка '"));
        Serial.print(key);
        Serial.println(F("' может быть изменена только через WebSocket."));
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