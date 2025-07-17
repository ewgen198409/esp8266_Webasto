void webasto() {
  // Эта функция управляет процессом горения

  static unsigned long timer;
  static unsigned long fan_timer;

  // Если текущее время меньше времени таймера, обновить таймер
  if (millis() < timer) {
    timer = millis();
  }

  static float temp_init;
  static int ignit_fail;
  static int seconds;
  static int cooled_off = 0;

  attempt = ignit_fail + 1;

  // Каждую секунду выполнять следующее
  if (millis() > timer + 1000) {
    timer = millis();
    seconds++; // увеличить счетчик секунд
    logging(ignit_fail, temp_init, seconds);
  }

  // Если все работает нормально
  if (!webasto_fail) {

    // Если пользователь нажал кнопку обогревателя и он выключен
    // то запускаем нагреватель (burn_mode = 1)
    if ((burn_mode == 0 || burn_mode == 3) && burn) {
      burn_mode = 1;
      seconds = 0;
      glow_time = 0;
      temp_init = exhaust_temp; // сохранить температуру выхлопа перед попыткой запуска
      cooled_off = 1;
    }

    // Если пользователь нажал кнопку обогревателя и он включен
    // то запускаем выключение (burn_mode = 3)
    if ((burn_mode == 1 || burn_mode == 2) && !burn) {
      burn_mode = 3;
      seconds = 0;
      ignit_fail = 0;
    }

    // Если было более 2 неудачных попыток запуска
    if (ignit_fail > 2) {
      webasto_fail = 1;
      burn_mode = 3;
      burn = 0;
    }

    // Если перегрев
    if (exhaust_temp > heater_overheat) {
      webasto_fail = 1;
      message = "Overheat";
      if (exhaust_temp < 150) {
        burn_mode = 3;
        burn = 0;
      }
    }

  } else { // Если произошла серьезная ошибка, остановить все
    burn = 0;
    ignit_fail = 0;
    message = "Off";
  }

  // Управление режимами горения
  switch (burn_mode) {
    case 0: { // Все выключено

        // Плавное снижение скорости вентилятора, если она больше нуля
        if (fan_speed > 0) {
            fan_speed *= 0.95f; // Уменьшаем скорость на 5% от текущего значения
            if (fan_speed < 0.5f) { // Практически достигли нуля
                fan_speed = 0;
            }
        } else {
            fan_speed = 0; // Гарантированно устанавливаем 0, если скорость <= 0
        }

        fuel_need = 0;
        glow_time = 0;
        lean_burn = 0;
      } break;

    case 1: { // Последовательность запуска огня
        if(webasto_fail)
          burn_mode = 3;

        // Охлаждение до 70 градусов
        if(exhaust_temp > 70 && (cooled_off == 0 || seconds < 5)) {
          message = "Cooling<70";

          // Плавное увеличение вентилятора до 90%
          float target_speed = 90.0f;
          float difference = target_speed - fan_speed;
          if (fabs(difference) > 0.05f) {  // Если еще не достигли почти 90%
              fan_speed += difference * 0.001f;  // Очень медленное увеличение
          } else {
              fan_speed = target_speed;  // Фиксируем 90%, если близко
          }

          fuel_need = 0;
          seconds = 0;
          cooled_off = 0;
        } else if(exhaust_temp <= 70) {
          cooled_off = 1;
        }

        // Очистка камеры
        if(seconds > 0 && seconds < 5) {
          // Плавное увеличение скорости (например, на 5% за итерацию)
          if (fan_speed < prime_fan_speed) {
              fan_speed += (prime_fan_speed - fan_speed) * 0.005;
              // Проверка на достижение целевой скорости с небольшим допуском
              if (prime_fan_speed - fan_speed < 0.1) {
                  fan_speed = prime_fan_speed;
              }
          }

          fuel_need = 0;
          message = "Clearing";
        }

        // Подготовка к розжигу
        if(seconds >= 6 && seconds <= 9) {
              // Плавное увеличение скорости (например, на 5% за итерацию)
          if (fan_speed > start_fan_speed) {
              fan_speed -= (fan_speed - start_fan_speed) * 0.005;
              // Проверка на достижение целевой скорости
              if (fan_speed - start_fan_speed < 0.1) {
                  fan_speed = start_fan_speed;
              }
          }
          glow_time = 100;
          message = "Prime";
        }

        // Инициализация температуры и таймера вентилятора
        if (seconds >=40 && seconds <= 41) {
          temp_init = exhaust_temp;
          fan_timer = millis();
        }


        // подача топлива перед розжигом
        if (seconds > 38 && seconds < 40) {
          fuel_need = prime_ratio(exhaust_temp);
        } 

        // Розжиг
        if (seconds > 40) {
          fuel_need = start_fuel;
          message = "Firing Up";

          // Увеличение скорости вентилятора
          if(fan_speed < start_fan_speed) {
            if(millis() - fan_timer >= 333) {
              fan_speed += 1;
              fan_timer = millis();
            }
          } else {
            fan_speed = start_fan_speed;
          }
        }

        // Если температура выхлопа поднялась, значит огонь зажегся
        if (exhaust_temp - temp_init > 3 && seconds >=60) {
          burn_mode = 2;
          seconds = 0;
          // glow_time = 0;
          ignit_fail = 0;
          temp_init = exhaust_temp;
          fan_timer = millis();
          message = "Started";
        }

        // Если последовательность розжига не сработала, попробовать снова
        if ((seconds > 80 && ignit_fail > 0) || seconds > 80) {
          burn_mode = 0;
          ignit_fail ++;
          cooled_off = 0;
          message = "Restarting";
        }

        // Если пламя погасло во время горения
        if (exhaust_temp < exhaust_temp_sec[9]-1.0 && seconds >= 60) {
          burn_mode = 3;
          ignit_fail ++;
          cooled_off = 1;
          message = "Start Flameout";
        }

      } break;

    case 2: { // Управление пламенем
        // ========================================================================
        // Назначение final_fan_speed в зависимости от currentState
        // Настройка набора мощности в зависимости от выбранного режима работы нагревателя
        // ========================================================================

        if (currentState == 0) {                       // HIGH
            final_fan_speed = 100;                     // 75% ШИМ
            full_power_increment_time = 50;            // время разгона

        } else if (currentState == 1) {                // MID
            final_fan_speed = 80;                      // 60% ШИМ
            full_power_increment_time = 40;            // время разгона

        } else if (currentState == 2) {                // LOW
            final_fan_speed = 60;                      // 45% ШИМ
            full_power_increment_time = 30;            // время разгона
        }

        // =========================================================================

        // Если температура нагревателя ниже критической и нет ошибок
        if (exhaust_temp < heater_overheat && !webasto_fail) {

          // Постепенное увеличение скорости вентилятора
          if (fan_speed < final_fan_speed) {
            if(millis() - fan_timer >= 333) {
              message = "Inc Burn";
              fan_speed += (final_fan_speed-35.00)/full_power_increment_time/3;

              if (exhaust_temp > 100) {     // в рабочем режиме значения набора скорости и топлива будут отличаться от значений режима разогрева с нуля
                // ================ Настройка частоты насоса по достижении выбранного режима ========================
                // =============  в рабочем режиме толливо лучше переключать быстрее ======================================
                if (currentState == 0) {                                              // HIGH
                  fuel_need = 2.38;                                                   // 6.5герц

                } else if (currentState == 1) {                                       // MID
                  fuel_need = 2.20;                                                    // 6.0

                } else if (currentState == 2) {                                       // LOW
                  fuel_need = 1.82;                                                    // 5.0
                }
                // ======  настраивается начиная от low заканчивая high ============================================
                // ==================================================================================================
                // ============  при старте важна плавность регулировки ===========================================================
              } else {
                if (currentState == 0) {                                              // HIGH
                  fuel_need += (final_fuel-1.03)/full_power_increment_time/3;        // 6.5герц

                } else if (currentState == 1) {                                       // MID
                  fuel_need += (final_fuel-1.27)/full_power_increment_time/3;         // 6.0

                } else if (currentState == 2) {                                       // LOW
                  fuel_need += (final_fuel-1.67)/full_power_increment_time/3;         // 5.0
                }
              }

              fan_timer = millis();
              // выключаем свечу когда температура выше 70 градусов
              if (exhaust_temp > 70) {
                glow_time = 0;
              }
            }
          } else {
            fan_speed = final_fan_speed;
            running_ratio(exhaust_temp);
          }
        }

        // Если перегрев или ошибка
        if(webasto_fail || exhaust_temp > heater_overheat) {
          fan_speed = 100;
          fuel_need = 0;
          message = "Overheat";
          if (exhaust_temp < 150) {
            cooled_off = 1;
            burn = 0;
            burn_mode = 3;
          }
        }

        // Если пламя погасло
        if (exhaust_temp < 80 && exhaust_temp < exhaust_temp_sec[9]-3 && seconds >= 60) {
          burn = 0;
          burn_mode = 3;
          message = "Running Flameout";
        }

      } break;

    case 3: {  
                                                    // Затухание огня, выключение
        if(exhaust_temp > 150) {                           // пока температура выше 150
          // Плавное увеличение до 80%
          float target_speed = 80.0f;
          float difference = target_speed - fan_speed;

          if (fabs(difference) > 0.05f) {                    // Если еще не достигли почти 80%
              fan_speed += difference * 0.001f;              // Очень медленное увеличение
          } else {
              fan_speed = target_speed;                      // Фиксируем 80%, если близко
          }
        } else {                                          // если ниже 150
          glow_time = 0;                                  // выключаем свечу

          // Плавное снижение до 50%
          float target_speed = 50.0f;
          float difference = fan_speed - target_speed;

          if (fabs(difference) > 0.05f) {                     // Если еще не достигли почти 50%
              fan_speed -= difference * 0.001f;                // Очень медленное снижение
          } else {
              fan_speed = target_speed;                         // Фиксируем 50%, если близко
          }
        }
        glow_time = 10;
        if (seconds > 10) {
          glow_time = 0;
        }
        fuel_need = 0;
        message = "Shutting Down";
        if (seconds > 30 && exhaust_temp < 50) {           // если прошло больше 30сек и температура ниже 50 градусов то все выключаем
          burn_mode = 0;
          message = "OFF";
          glow_time = 0;
        }
      } break;
  }

  // Вызов функций для управления насосом, вентилятором и свечой накаливания
  fuel_pump();
  burn_fan();
  glow_plug();
}
