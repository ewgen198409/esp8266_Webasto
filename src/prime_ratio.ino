float prime_ratio(float exhaust_temp) {
  // Если температура выхлопа меньше или равна нижней границе, возвращаем коэффициент для низкой температуры
  if(exhaust_temp <= prime_low_temp)
    return prime_low_temp_fuelrate;

  // Если температура выхлопа больше или равна верхней границе, возвращаем коэффициент для высокой температуры
  if(exhaust_temp >= prime_high_temp)
    return prime_high_temp_fuelrate;

  // Вычисляем количество шагов между нижней и верхней границами температуры
  float steps = prime_high_temp - prime_low_temp;

  // Вычисляем изменение коэффициента подачи топлива на один шаг температуры
  float fuel_steps = (prime_low_temp_fuelrate - prime_high_temp_fuelrate) / steps;

  // Возвращаем коэффициент подачи топлива, линейно интерполированный между нижней и верхней границами
  return prime_low_temp_fuelrate - ((exhaust_temp - prime_low_temp) * fuel_steps);
}
