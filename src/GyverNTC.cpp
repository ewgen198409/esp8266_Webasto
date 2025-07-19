#include "GyverNTC.h"
#include <math.h> // Для isnan() и log()

float NTC_computeRR(float raw_adc_value, float baseDiv, uint16_t B, uint8_t t, uint8_t res, double adc_vcc) {
    // Проверяем на некорректные входные данные
    if (raw_adc_value <= 0 || isnan(raw_adc_value)) return INFINITY;

    float max_adc_val = (float)((1 << res) - 1); // Максимальное значение АЦП (например, 1023 для 10 бит)

    // Вычисляем напряжение на NTC-термисторе
    float V_NTC = raw_adc_value * (adc_vcc / max_adc_val);

    // Вычисляем напряжение на последовательном резисторе
    float V_series_res = adc_vcc - V_NTC;

    // Проверяем на деление на ноль или отрицательное напряжение на резисторе
    if (V_series_res <= 0.0f) return INFINITY; // Возвращаем бесконечность в случае ошибки

    // Вычисляем отношение сопротивления NTC к номинальному сопротивлению R0.
    // R_NTC / R_series = V_NTC / V_series_res
    // R_NTC / R0 = (R_series / R0) * (V_NTC / V_series_res)
    // baseDiv = R_series / R0
    float resistance_ratio = baseDiv * (V_NTC / V_series_res);

    // Проверяем на неположительное значение для логарифма
    if (resistance_ratio <= 0.0f) return INFINITY;

    // Применяем уравнение Стейнхарта-Харта для вычисления температуры в Кельвинах
    float log_resistance_ratio = log(resistance_ratio);
    float inv_temp_K = (log_resistance_ratio / B) + 1.0f / (t + 273.15f);

    // Преобразуем температуру из Кельвинов в градусы Цельсия
    return (1.0f / inv_temp_K - 273.15f);
}

float NTC_compute(float analog, uint32_t R, uint16_t B, uint8_t t, uint32_t Rt, uint8_t res, double adc_vcc) {
    // Вызываем NTC_computeRR, передавая все необходимые параметры, включая adc_vcc
    return NTC_computeRR(analog, (float)R / Rt, B, t, res, adc_vcc);
}
