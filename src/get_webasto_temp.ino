#include <GyverNTC.h> // Включаем библиотеку GyverNTC
#include <Arduino.h>  // Для использования Serial.print и analogRead

// --- Глобальные константы для настройки NTC датчика ---
// ОБНОВЛЕНО: NTC_R0 и NTC_B скорректированы для лучшей калибровки
const uint32_t NTC_R0 = 50654UL;        // Номинальное сопротивление NTC при 25°C (в Омах), UL для unsigned long literal
const uint16_t NTC_B = 4430;            // B-коэффициент NTC (из даташита терморезистора)
const uint32_t NTC_SERIES_RESISTOR = 17000UL; // Сопротивление последовательного резистора (в Омах)
const uint16_t ADC_BITS_RESOLUTION = 10; // Разрешение ADC в битах (10 для 0-1023)
const double ADC_VCC_VOLTAGE = 1.0;      // Опорное напряжение АЦП в вольтах (1.0V для вашего аналогового входа)

// --- Глобальные переменные для логирования (доступны в других файлах как extern) ---
float raw_adc_value_global = 0.0f;
float calculated_resistance_global = 0.0f;

/**
 * @brief Читает значение температуры с датчика NTC.
 * * Сохраняет сырое значение АЦП и рассчитанное сопротивление в глобальные переменные.
 * * @param temp_pin Аналоговый пин, подключенный к NTC терморезистору.
 * @param exhaust Флаг (сохранен для совместимости, не используется в расчете).
 * @return Температура в градусах Цельсия или -999.0f в случае ошибки.
 */
float get_wabasto_temp(int temp_pin, int exhaust) {
    // Статический объект GyverNTC для инициализации только один раз.
    // Порядок NTC_SERIES_RESISTOR и NTC_R0 в конструкторе GyverNTC корректен.
    static GyverNTC ntc(
        (uint8_t)temp_pin,      // Аналоговый пин
        NTC_SERIES_RESISTOR,    // Сопротивление последовательного резистора (R)
        NTC_B,                  // B-коэффициент NTC
        25,                     // Температура, при которой R0 (по умолчанию 25°C)
        NTC_R0,                 // Номинальное сопротивление NTC при 25°C (Rt)
        ADC_BITS_RESOLUTION,    // Разрешение ADC в битах (например, 10 для 0-1023)
        ADC_VCC_VOLTAGE         // ПЕРЕДАЕМ ВАШЕ ОПОРНОЕ НАПРЯЖЕНИЕ АЦП (1.0V)
    );

    // --- Расчет сырого значения АЦП и рассчитанного сопротивления для логирования ---
    raw_adc_value_global = analogRead(temp_pin); // Считываем сырое значение АЦП и сохраняем глобально

    // Рассчитываем сопротивление NTC по формуле делителя напряжения:
    // R_NTC = R_SERIES * (ADC_VALUE / (ADC_MAX_VALUE - ADC_VALUE))
    float max_adc_val_float = (float)((1 << ADC_BITS_RESOLUTION) - 1); // Максимальное значение АЦП (например, 1023)

    // Проверяем, чтобы избежать деления на ноль или некорректных значений
    if (raw_adc_value_global >= 0 && raw_adc_value_global < max_adc_val_float) {
        // Здесь используется ADC_VCC_VOLTAGE для корректного масштабирования
        // Хотя GyverNTC делает это внутри, для логирования мы делаем это вручную.
        float v_ntc_measured = raw_adc_value_global * (ADC_VCC_VOLTAGE / max_adc_val_float);
        float v_series_res_measured = ADC_VCC_VOLTAGE - v_ntc_measured;

        if (v_series_res_measured > 0.0f) {
            calculated_resistance_global = NTC_SERIES_RESISTOR * (v_ntc_measured / v_series_res_measured);
        } else {
            calculated_resistance_global = -1.0f; // Индикатор ошибки
        }
    } else if (raw_adc_value_global == max_adc_val_float) {
        calculated_resistance_global = 0.0f; // При максимальном АЦП сопротивление NTC очень низкое (почти КЗ)
    } else {
        calculated_resistance_global = -1.0f; // Индикатор ошибки или некорректного чтения
    }
    // --- Конец секции расчета для логирования ---

    float temperature_celsius = ntc.getTemp(); // Получаем температуру от библиотеки GyverNTC

    // --- Обработка возможных значений ошибки от библиотеки GyverNTC ---
    if (temperature_celsius == 85.0f || temperature_celsius == -127.0f) {
        return -999.0f; // Возвращаем специфический код ошибки
    }
    
    return temperature_celsius;
}
