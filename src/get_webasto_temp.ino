#include <GyverNTC.h>

// --- Константы настройки NTC ---
const uint32_t NTC_R0 = 50000;       // Номинальное сопротивление при 25°C (Ом)
const uint16_t NTC_B  = 3950;        // B-коэффициент
const uint32_t NTC_SERIES_RESISTOR = 50000; // Последовательный резистор (Ом)
const uint16_t ADC_BITS_RESOLUTION = 10;    // Разрешение АЦП (10 бит = 0-1023)

float get_wabasto_temp(int temp_pin, int exhaust) {
    static GyverNTC ntc(
        (uint8_t)temp_pin,       // Аналоговый пин
        NTC_SERIES_RESISTOR,     // R
        NTC_B,                   // B
        25,                      // Температура при которой Rt
        NTC_R0,                  // Rt
        ADC_BITS_RESOLUTION      // Разрешение АЦП
    );  // ⚡️ тут уже 6 аргументов, как нужно

    float temperature_celsius = ntc.getTemp();

    if (temperature_celsius == 250.0f || temperature_celsius == -127.0f) {
        return -999.0f;  // Ошибка
    }
    return temperature_celsius;
}