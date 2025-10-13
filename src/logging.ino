
extern float total_fuel_consumed_liters;
extern float fuel_consumption_per_hour;
extern float ds18b20_temp;

void logging(int ignit_fail, float temp_init, int seconds) {
  if (settingsUpdateInProgress) {
    return; // Пропускаем выполнение, если идёт обновление настроек
  }
    Serial.print(" | F: ");
    Serial.print(webasto_fail);
    Serial.print(" | IgnF#: ");
    Serial.print(ignit_fail);
    Serial.print(" | ETmp: ");
    Serial.print(exhaust_temp);
    if(burn_mode == 1)
    {
      Serial.print("/");
      Serial.print(temp_init+3);
    }
    Serial.print(" | Fan%: ");
    Serial.print(fan_speed);
    Serial.print(" | FHZ ");
    if(delayed_period>0)
      Serial.print(1000.00/delayed_period);
    Serial.print(" | FN: ");
    Serial.print(fuel_need);
    Serial.print(" | Gl: ");
    Serial.print(debug_glow_plug_on);
    Serial.print(" | CyTim: ");
    Serial.print(seconds);
    Serial.print(" | I: ");
    Serial.println(message);
    Serial.print(" | FinalFuel: ");
    Serial.print(final_fuel);
    Serial.print(" | TFC: ");
    Serial.print(total_fuel_consumed_liters, 3);
    Serial.print(" | FCH: ");
    Serial.print(fuel_consumption_per_hour, 3);
    Serial.print(" | St: ");
    Serial.print(currentState);
    Serial.print(" | InTemp: ");
    Serial.println(ds18b20_temp);

}
