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
  }

  // Вычисляем коэффициент деления насоса
  double pump_division = 60.00 / pump_size;

  // Вычисляем задержку между импульсами
  delayed_period = 1000 / fuel_need / pump_division;
  // Изменение на частоту потребности в топливе, добавление переключателя объема насоса для разных насосов

  // Скважность 15%
  int duty_cycle = 15;

  // Вычисляем время включения и выключения насоса
  int on_time = (delayed_period * duty_cycle) / 100;
  int off_time = delayed_period - on_time;

  // Если время начала импульса меньше или равно текущему времени, выключаем насос
  if(pulse_started_off_at <= millis())
    digitalWrite(fuel_pump_pin, LOW);

  // Если время следующего импульса меньше или равно текущему времени, включаем насос
  if(next_pulse_timer <= millis())
  {
      // Устанавливаем время следующего импульса
      next_pulse_timer = off_time + on_time + millis();
      // Включаем насос
      digitalWrite(fuel_pump_pin, HIGH);
      // Устанавливаем время начала импульса
      pulse_started_off_at = millis() + on_time;
  }
}
