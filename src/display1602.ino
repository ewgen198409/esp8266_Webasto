void display_data() {
  // Check if exhaust_temp has changed
  if ((int)exhaust_temp != prev_exhaust_temp) {
    lcd.setCursor(0, 0);
    lcd.print(F("    ")); // Очистка
    lcd.setCursor(0, 0);
    lcd.print((int)exhaust_temp);
    lcd.print(F("\xDF")); // Вывод значка градуса после значения температуры
    prev_exhaust_temp = (int)exhaust_temp;
  }

  // Check if fan_speed has changed
  if ((int)fan_speed != prev_fan_speed) {
    lcd.setCursor(5, 0);
    lcd.print(F(" ")); // Очистка символа заполнения
    lcd.setCursor(5, 0);
    int fillIndex = (fan_speed == 0) ? 7 : map((int)fan_speed, 1, 100, 1, 6);
    lcd.write(fillIndex);

    lcd.setCursor(6, 0);
    lcd.print(F("    ")); // Очистка процентов
    lcd.setCursor(6, 0);
    lcd.print((int)fan_speed);
    lcd.print(F("%"));
    prev_fan_speed = (int)fan_speed;
  }

  // Check if delayed_period has changed
  // Используем проверку на 0, чтобы избежать деления на ноль и isinf()
  if (delayed_period == 0) {
    if (prev_delayed_period != -1.0) { // Если ранее не было "---" (бесконечности)
      lcd.setCursor(10, 0);
      lcd.print(F("F:---"));
      prev_delayed_period = -1.0; // Устанавливаем специальное значение для "бесконечности"
    }
  } else {
    float value = 1000.00 / delayed_period;
    // Сравниваем с небольшой дельтой для чисел с плавающей запятой
    if (abs(value - prev_delayed_period) > 0.1 || prev_delayed_period == -1.0) {
      lcd.setCursor(10, 0);
      lcd.print(F("F:"));
      lcd.print(value, 1);
      prev_delayed_period = value;
    }
  }

  // Обновление дисплея только при изменении сообщения
  // Используем strcmp для сравнения C-строк
  if (strcmp(message, prev_message) != 0) {
    lcd.setCursor(0, 1);
    lcd.print(F("                ")); // Очистка второй строки
    lcd.setCursor(0, 1);
    lcd.print(message); // Вывод нового сообщения
    prev_message = message; // Обновление предыдущего сообщения (копируем указатель)

    // Отображение значения attempt при сообщении "Firing Up"
    if (strcmp(message, "Firing Up") == 0) {
      lcd.setCursor(15, 1); // Устанавливаем курсор на правую сторону нижней строки
      lcd.print(attempt);
    }
    // При изменении сообщения сбрасываем предыдущее состояние systemState для принудительного обновления
    prev_currentState = -1;
  }

  // Check if burn_mode has changed
  if (burn_mode != prev_burn_mode) {
    lcd.setCursor(0, 1);
    lcd.print(F("                ")); // Очистка
    lcd.setCursor(0, 1);
    switch (burn_mode) {
      case 0:
        lcd.print(F("OFF"));
        break;

      case 1:
        if(strcmp(message, "Cooling<70") == 0) { // Используем strcmp
          lcd.print(F("Cooling<70"));
        } else {
          lcd.print(F("Starting"));
        }
        break;

      case 2:
        lcd.print(F("Running"));
        break;
      case 3:
        lcd.print(F("Shuting Down"));
        break;
    }
    prev_burn_mode = burn_mode;
    prev_currentState = -1;
  }

  // Check if webasto_fail is 1 and burn_mode is OFF
  if (webasto_fail == 1 && burn_mode == 0) {
    if (millis() - fail_blink_timer >= 500) { // Мигание каждые 500 миллисекунд
      fail_blink_timer = millis();
      fail_displayed = !fail_displayed;

      lcd.setCursor(11, 1); // Устанавливаем курсор на правую сторону нижней строки
      if (fail_displayed) {
        lcd.print(F("FAIL"));
      } else {
        lcd.print(F("    ")); // Очищаем место для надписи "FAIL"
      }
    }
  } else {
    // Если webasto_fail не активен или burn_mode не OFF, убедимся, что "FAIL" не отображается
    if (fail_displayed) { // Только если "FAIL" был отображен, очищаем его
        lcd.setCursor(11, 1);
        lcd.print(F("    "));
        fail_displayed = false;
    }
    fail_blink_timer = millis(); // Сбрасываем таймер, чтобы мигание начиналось с нуля при следующем сбое
  }


  // отображаем символ молнии когда свеча включена
  // Проверка изменения debug_glow_plug_on
  if (debug_glow_plug_on != prev_debug_glow_plug_on) { // ИСПРАВЛЕНО: prev_debug_plug_on -> prev_debug_glow_plug_on
    lcd.setCursor(15, 0); // Установка курсора на 16-ю позицию первой строки
    if (debug_glow_plug_on) { // Используем bool напрямую
      lcd.print(char(244)); // Вывод символа молнии
    } else {
      lcd.print(F(" ")); // Очистка символа
    }
    prev_debug_glow_plug_on = debug_glow_plug_on; // ИСПРАВЛЕНО: prev_debug_plug_on -> prev_debug_glow_plug_on
  }

  // Отображение currentState в правой части нижней строки (только при изменении)
  if ((currentState != prev_currentState) && (webasto_fail == 0)) {
    lcd.setCursor(12, 1); // Устанавливаем курсор на правую сторону нижней строки
    lcd.print(F("    ")); // Очищаем область (4 символа)
    lcd.setCursor(12, 1);

    switch(currentState) {
      case 0:
        lcd.print(F("FULL"));
        break;
      case 1:
        lcd.print(F(" MID"));
        break;
      case 2:
        lcd.print(F(" LOW"));
        break;
      default:
        lcd.print(F("    ")); // На случай неожиданных значений
    }
    prev_currentState = currentState;
  }
}