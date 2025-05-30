void running_ratio(float exhaust_temp) {
  // Статическая переменная для отслеживания времени изменения
  static unsigned long change_timer;

  // Целевое и текущее значение топлива
  int fuel_target = final_fuel * 100;  // Инициализация значением по умолчанию
  int fuel_current;
  // =====================================================================================================
  // =============== Настройка топлива в зависимости от выбранного режима ================================
  // =====================================================================================================
  if (currentState == 0) {                                              // HIGH  100% (75% ШИМ)
    final_fuel = 2.38;                                                   // 6.5 герц
    throttling_high_fuel = 2.20;                                         // 6.0 герц
    throttling_middle_fuel = 2.01;                                       // 5.5 герц
    throttling_low_fuel = 1.65;                                          // 4.5 герц

  } else if (currentState == 1) {                                       // MID  80% (60% ШИМ)
    final_fuel = 2.20;                                                   // 6.0 герц
    throttling_high_fuel = 2.01;                                         // 5.5 герц
    throttling_middle_fuel = 1.81;                                       // 5.0 герц
    throttling_low_fuel = 1.46;                                          // ~4.0 герц

  } else if (currentState == 2) {                                       // LOW  60% (45% ШИМ)
    final_fuel = 1.81;                                                   // 5.0 герц
    throttling_high_fuel = 1.65;                                         // 4.5 герц
    throttling_middle_fuel = 1.46;                                       // 4.0 герц
    throttling_low_fuel = 1.08;                                          // ~3.0 герц
  }
  // =====================================================================================================


  // Если температура выхлопа больше 150, начинаем регулировку (Переход в режим работы !!!)
  if (exhaust_temp >= 150) {
    // Вычисляем текущее значение топлива
    fuel_current = fuel_need * 100;

    // Определяем целевое значение топлива в зависимости от температуры нагревателя
    if ((exhaust_temp * 100 > heater_min * 100) && (exhaust_temp * 100 < heater_target * 100)) {
      // Если температура нагревателя выше 190 и меньше 195, устанавливаем высокое значение топлива
      fuel_target = throttling_high_fuel * 100;
    }

    if ((exhaust_temp * 100 > heater_target * 100) && (exhaust_temp * 100 < heater_warning * 100)) {
      // Если температура нагревателя выше 195 и меньше 200, устанавливаем высокое значение топлива
      fuel_target = throttling_middle_fuel * 100;
    }

    if ((exhaust_temp * 100 > heater_warning * 100) && (exhaust_temp * 100 < heater_overheat * 100)) {
        // Если температура выхлопа выше 200 и меньше 210 устанавливаем высокое значение топлива
      fuel_target = throttling_low_fuel * 100;
    }


    // Инициализируем таймер изменения, если он еще не был установлен
    if (change_timer == 0) {
      change_timer = millis();
    }

    // Если целевое значение топлива отличается от текущего
    if (fuel_target != fuel_current) {
      // Проверяем, прошло ли достаточно времени для изменения
      if (millis() - change_timer >= 150) {
        // Увеличиваем или уменьшаем значение топлива в зависимости от необходимости
        if (fuel_current < fuel_target) {
          fuel_need += 0.01;
          message = "Fuel UP";
        } else if (fuel_current > fuel_target) {
          fuel_need -= 0.01;
          message = "Fuel DOWN";
        } else {
          message = "Running";
        }
        // Обновляем таймер изменения
        change_timer = millis();
      }
    } else {
      // Если целевое значение топлива совпадает с текущим, продолжаем работу без изменений
      message = "Running";
    }
  }
}
