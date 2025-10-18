// Объявление внешних глобальных переменных
extern float fuel_need;
extern int pump_size;
extern int delayed_period;

extern float total_fuel_consumed_liters;
extern float fuel_consumption_per_hour;

extern int fuel_pump_pin;
extern Settings settings;

// Константы для генерации импульсов
#define DUTY_CYCLE 15           // Фиксированная скважность 15%
#define MIN_FREQUENCY 1.0       // Минимальная частота 1 Гц
#define MAX_FREQUENCY 10.0      // Максимальная частота 10 Гц
#define MIN_FUEL_NEED 0.01      // Минимальное значение fuel_need для работы (0.01 = точность 2 знака)

// Переменные для генерации импульсов
static unsigned long pulse_period_ms = 1000;  // Период импульса в миллисекундах
static unsigned long pulse_on_time_ms = 150;  // Время включения (15% от периода)
static unsigned long last_pulse_start = 0;    // Время начала последнего импульса
static bool pulse_active = false;             // Флаг активного импульса

void setup_fuel_pump() {
  pinMode(fuel_pump_pin, OUTPUT);
  digitalWrite(fuel_pump_pin, LOW);
  pulse_active = false;
  last_pulse_start = millis();
}

void fuel_pump() {
  unsigned long current_time = millis();
  
  // Если топливо не требуется или значение слишком мало
  if(fuel_need < MIN_FUEL_NEED)
  {
    // Безопасное выключение
    if(pulse_active) {
      digitalWrite(fuel_pump_pin, LOW);
      pulse_active = false;
    }
    
    // Сбрасываем параметры
    fuel_consumption_per_hour = 0.0;
    delayed_period = 0;
    last_pulse_start = current_time; // Обновляем для предотвращения скачка при запуске
    return;
  }

  // Вычисляем требуемую частоту импульсов на основе fuel_need
  // Защита от деления на ноль и слишком малых значений
  float calculated_period = 1000.0 / fuel_need / (60.0 / pump_size);
  
  // Проверка на валидность расчета
  if(calculated_period <= 0 || isnan(calculated_period) || isinf(calculated_period)) {
    calculated_period = 1000; // Значение по умолчанию (1 Гц)
  }
  
  delayed_period = (int)calculated_period;
  
  // Вычисляем частоту в Гц и ограничиваем в допустимых пределах
  float target_frequency = 1000.0 / delayed_period;
  target_frequency = constrain(target_frequency, MIN_FREQUENCY, MAX_FREQUENCY);
  
  // Вычисляем период импульса в миллисекундах
  pulse_period_ms = (unsigned long)(1000.0 / target_frequency);
  
  // Защита от нулевого периода
  if(pulse_period_ms < 100) pulse_period_ms = 100; // Минимум 100 мс (10 Гц)
  
  // Вычисляем время включения (15% от периода)
  pulse_on_time_ms = (pulse_period_ms * DUTY_CYCLE) / 100;
  
  // Защита от слишком короткого импульса
  if(pulse_on_time_ms < 10) pulse_on_time_ms = 10; // Минимум 10 мс

  // Безопасное вычисление времени в цикле
  unsigned long time_in_cycle = current_time - last_pulse_start;

  // Если начался новый цикл импульса
  if(time_in_cycle >= pulse_period_ms)
  {
    // Сбрасываем счетчик цикла
    last_pulse_start = current_time;
    time_in_cycle = 0;
    
    // Включаем импульс
    digitalWrite(fuel_pump_pin, HIGH);
    pulse_active = true;

    // === Расчет потребленного топлива ===
    // Один импульс = одна порция топлива
    float ml_per_pulse = settings.pump_size / 1000.0;
    
    // Защита от отрицательных или аномальных значений
    if(ml_per_pulse > 0 && ml_per_pulse < 1000) { // Разумный диапазон
      total_fuel_consumed_liters += ml_per_pulse / 1000.0;

      // Обновляем расход топлива за час
      if (pulse_period_ms > 0) {
          float pulses_per_second = 1000.0 / pulse_period_ms;
          fuel_consumption_per_hour = (pulses_per_second * ml_per_pulse * 3600.0) / 1000.0;
      } else {
          fuel_consumption_per_hour = 0.0;
      }
    }
  }

  // Если время включения истекло, выключаем импульс
  if(pulse_active && time_in_cycle >= pulse_on_time_ms)
  {
    digitalWrite(fuel_pump_pin, LOW);
    pulse_active = false;
  }
}