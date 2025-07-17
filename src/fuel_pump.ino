// Объявление внешних глобальных переменных, которые должны быть определены в основном файле (например, low_smoke.ino)
extern float fuel_need;
extern int pump_size; // Предполагается, что pump_size - это количество топлива в миллилитрах за 1000 импульсов
extern int delayed_period; // Задержка между импульсами, используется для расчета частоты

// Новые глобальные переменные для учета расхода топлива
extern float total_fuel_consumed_liters; // Общее количество потребленного топлива в литрах
extern float fuel_consumption_per_hour;  // Средний расход топлива за 1 час в литрах/час

// Объявление extern для fuel_pump_pin
extern int fuel_pump_pin;

// Объявление extern для структуры Settings (если она определена в другом файле)
extern Settings settings;


void fuel_pump() {
  // Статические переменные для хранения времени следующего импульса и времени начала импульса
  static unsigned long next_pulse_timer;
  static unsigned long pulse_started_off_at;

  // Если топливо не требуется (fuel_need == 0), устанавливаем таймеры
  if(fuel_need == 0)
  {
    // Устанавливаем время следующего импульса на 50 миллисекунд вперед
    next_pulse_timer = millis() + 50;
    // Устанавливаем время начала импульса на 50 миллисекунд назад
    pulse_started_off_at = millis() - 50;
    fuel_consumption_per_hour = 0.0; // Сбрасываем расход за час, если нет потребности в топливе
  }

  // Вычисляем задержку между импульсами
  // ВНИМАНИЕ: В этой формуле '(60.00 / pump_size)' 'pump_size' интерпретируется как
  // "количество импульсов, необходимых для подачи 60 мл топлива".
  // Если ваше 'pump_size' на самом деле означает "миллилитры за 1000 импульсов",
  // то эта часть формулы может привести к некорректному расчету 'delayed_period'.
  // Логика этой строки сохранена без изменений, как было запрошено.
  delayed_period = 1000 / fuel_need / (60.00 / pump_size); 

  // Скважность 15%
  int duty_cycle = 15;

  // Вычисляем время включения насоса
  int on_time = (delayed_period * duty_cycle) / 100;

  // Если время начала импульса меньше или равно текущему времени, выключаем насос
  if(pulse_started_off_at <= millis())
    digitalWrite(fuel_pump_pin, LOW);

  // Если время следующего импульса меньше или равно текущему времени, включаем насос
  if(next_pulse_timer <= millis())
  {
      // Устанавливаем время следующего импульса
      next_pulse_timer = (on_time + (delayed_period - on_time)) + millis(); 
      // Включаем насос
      digitalWrite(fuel_pump_pin, HIGH);
      // Устанавливаем время начала импульса
      pulse_started_off_at = millis() + on_time;

      // === Расчет потребленного топлива ===
      // Теперь 'settings.pump_size' используется как "миллилитры за 1000 импульсов".
      // Следовательно, объем за один импульс = settings.pump_size / 1000.0.
      float ml_per_pulse = settings.pump_size / 1000.0; // ИСПРАВЛЕНО: Учитываем 1000 импульсов

      // Увеличиваем общее потребление топлива
      total_fuel_consumed_liters += ml_per_pulse / 1000.0; // Добавляем в литрах

      // Обновляем расход топлива за час
      // Текущая частота импульсов: 1000.0 / delayed_period (импульсов в секунду)
      // Расход в мл/сек: (1000.0 / delayed_period) * ml_per_pulse
      // Расход в мл/час: (1000.0 / delayed_period) * ml_per_pulse * 3600.0
      // Расход в л/час: ((1000.0 / delayed_period) * ml_per_pulse * 3600.0) / 1000.0
      // Упрощаем: (ml_per_pulse * 3.6) / delayed_period
      
      // Чтобы избежать деления на ноль, если delayed_period внезапно станет 0
      if (delayed_period > 0) {
          fuel_consumption_per_hour = (ml_per_pulse * 3600.0) / delayed_period; // л/час
      } else {
          fuel_consumption_per_hour = 0.0;
      }
      
      // Для экономии ресурсов и уменьшения частоты записи в EEPROM,
      // сохраняем настройки только при значительном изменении или периодически.
      // Пока не будем вызывать saveSettings() здесь, чтобы не нагружать EEPROM.
      // Это должно быть сделано в loop() основного файла с определенной периодичностью.
  }
}

