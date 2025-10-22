

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
      final_fuel = 2.38;          // для правильной регулировки топлива на старте
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

        fan_speed = 0;
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

        // Очистка камеры - равномерное увеличение скорости за 5 секунд
        if(seconds > 0 && seconds < 5) {
          // Рассчитываем требуемую скорость на основе прогресса времени
          float progress = seconds / 5.0;  // Прогресс от 0 до 1 за 5 секунд
          float target_speed = prime_fan_speed * progress;

          // Плавно устанавливаем скорость
          if (fan_speed < target_speed) {
              fan_speed += (target_speed - fan_speed) * 0.3;  // Быстрое приближение
          } else {
              fan_speed = target_speed;
          }

          // Если достигли целевой скорости на 5-й секунде
          if (seconds >= 4) {
              fan_speed = prime_fan_speed;
          }

          fuel_need = 0;
          message = "Clearing";
        }

        // Подготовка к розжигу
        if(seconds >= 6 && seconds <= 11) {
          if (fan_speed > start_fan_speed) {
              fan_speed = start_fan_speed;
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
        if (seconds > 37 && seconds < 40) {
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

        // Если температура выхлопа поднялась на 3 градуса, значит огонь зажегся
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

          // Постепенное изменение скорости вентилятора
          if (fan_speed < final_fan_speed) {
            // Увеличение скорости вентилятора и топлива одновременно
            if(millis() - fan_timer >= 1000) {  // Каждую секунду
              message = "Inc Burn";

              // Определяем целевые значения
              float target_fan_speed = final_fan_speed;
              float target_fuel = 0;
              if (currentState == 0) {                                              // HIGH
                target_fuel = 2.38;
              } else if (currentState == 1) {                                       // MID
                target_fuel = 2.20;
              } else if (currentState == 2) {                                       // LOW
                target_fuel = 1.91;
              }

              // Увеличиваем вентилятор на 3% от текущего значения
              float fan_increment = fan_speed * 0.03;
              float old_fan_speed = fan_speed;
              fan_speed += fan_increment;

              // Если увеличили слишком сильно, устанавливаем целевое значение
              if(fan_speed > target_fan_speed) {
                fan_speed = target_fan_speed;
              }

              // Синхронно изменяем топливо пропорционально изменению вентилятора
              float fan_change_ratio = (fan_speed - old_fan_speed) / (target_fan_speed - old_fan_speed);

              // Рассчитываем новое значение топлива на основе прогресса вентилятора
              float new_fuel_need = fuel_need + (target_fuel - fuel_need) * fan_change_ratio;

              // Если топливо превысило максимум для режима
              if(new_fuel_need > target_fuel) {
                fuel_need = target_fuel;
              } else {
                fuel_need = new_fuel_need;
              }

              fan_timer = millis();
            }

          } else if (fan_speed > final_fan_speed) {
            // Уменьшение скорости вентилятора и топлива одновременно
            if(millis() - fan_timer >= 1000) {  // Каждую секунду
              message = "Dec Burn";

              // Определяем целевые значения
              float target_fan_speed = final_fan_speed;
              float target_fuel = 0;
              if (currentState == 0) {                                              // HIGH
                target_fuel = 2.38;
              } else if (currentState == 1) {                                       // MID
                target_fuel = 2.20;
              } else if (currentState == 2) {                                       // LOW
                target_fuel = 1.91;
              }

              // Уменьшаем вентилятор на 5% от текущего значения
              float fan_decrement = fan_speed * 0.05;
              float old_fan_speed = fan_speed;
              fan_speed -= fan_decrement;

              // Если уменьшили слишком сильно, устанавливаем целевое значение
              if(fan_speed < target_fan_speed) {
                fan_speed = target_fan_speed;
              }

              // Синхронно изменяем топливо пропорционально изменению вентилятора
              float fan_change_ratio = (old_fan_speed - fan_speed) / (old_fan_speed - target_fan_speed);

              // Рассчитываем новое значение топлива на основе прогресса вентилятора
              float new_fuel_need = fuel_need - (fuel_need - target_fuel) * fan_change_ratio;

              // Если топливо ушло в отрицательную область или слишком сильно уменьшилось
              if(new_fuel_need < target_fuel) {
                fuel_need = target_fuel;
              } else {
                fuel_need = new_fuel_need;
              }

              fan_timer = millis();
            }

          } else if (exhaust_temp > 100) {    // когда температура достигла 100 градусов
              fan_speed = final_fan_speed;
              running_ratio(exhaust_temp);
          }
        }

        // выключаем свечу когда температура выше 70 градусов
        if (exhaust_temp > 70) {
          glow_time = 0;
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
        if(exhaust_temp > 100) {                           // пока температура выше 100
          // Плавное увеличение до 80%
          float target_speed = 80.0f;
          float difference = target_speed - fan_speed;

          if (fan_speed != target_speed) {                    // Если еще не достигли почти 80%
              fan_speed += difference * 0.1f;              // Очень медленное увеличение
          } else {
              fan_speed = target_speed;                      // Фиксируем 80%, если близко
          }
        } else {                                          // если ниже 150

          // Плавное снижение до 50%
          float target_speed = 50.0f;
          float difference = fan_speed - target_speed;

          if (fan_speed != target_speed) {                     // Если еще не достигли почти 50%
              fan_speed -= difference * 0.1f;                // Очень медленное снижение
          } else {
              fan_speed = target_speed;                         // Фиксируем 50%, если близко
          }
        }
        glow_time = 15;
        if (seconds > 15) {
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



// ### Краткий анализ логики по режимам:

// 1. __Case 0 (Выключено)__: Вентилятор плавно снижается до 0, топливо и свеча выключены.

// 2. __Case 1 (Запуск)__:

//    - Охлаждение камеры до ≤70°C (если нужно).
//    - Очистка (0-5 сек): вентилятор до prime_fan_speed.
//    - Подготовка (6-9 сек): вентилятор до start_fan_speed, свеча включена (glow_time = 100).
//    - Подача топлива перед розжигом (38-40 сек).
//    - Розжиг (>40 сек): топливо start_fuel, вентилятор увеличивается.
//    - Переход в горение (case 2) при подъеме температуры на 3°C.

// 3. __Case 2 (Горение)__:

//    - Настройка final_fan_speed и времени разгона по режиму (HIGH/MID/LOW).
//    - __Свеча выключается при temp >70°C__ (теперь проверяется всегда в начале режима).
//    - Увеличение вентилятора до final_fan_speed с одновременным ростом топлива.
//    - После достижения final_fan_speed: вызов running_ratio для регулировки топлива по температуре (если temp ≥100°C).
//    - Обработка перегрева: вентилятор на 100%, топливо 0, переход в выключение.
//    - Обработка погасания пламени: переход в выключение.

// 4. __Case 3 (Выключение)__:

//    - При temp >150°C: вентилятор до 80%.
//    - При temp ≤150°C: свеча выключается, вентилятор до 50%, glow_time = 10 (затем 0 после 10 сек).
//    - Полное выключение при temp <50°C и времени >30 сек.

// ### Потенциальные замечания:

// - В running_ratio (src/running_ratio.ino) регулировка топлива происходит только при temp ≥100°C, что корректно для поддержания режима.
// - Переходы между режимами логичны, с проверками на ошибки (webasto_fail, перегрев).
