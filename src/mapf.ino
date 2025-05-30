float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  // Функция для масштабирования значения x из одного диапазона в другой

  // Ограничиваем значение x в пределах от in_min до in_max
  // Это предотвращает выход за пределы допустимого диапазона входных значений
  x = constrain(x, in_min, in_max);

  // Вычисляем новое значение x, масштабируя его из диапазона [in_min, in_max] в [out_min, out_max]
  // Формула: (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min
  // Это линейное преобразование, которое переводит значение x из одного диапазона в другой
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
