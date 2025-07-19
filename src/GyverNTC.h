#ifndef _GyverNTC_h
#define _GyverNTC_h
#include <Arduino.h>

float NTC_computeRR(float analog, float baseDiv, uint16_t B, uint8_t t = 25, uint8_t res = 10, double adc_vcc = 3.3);

float NTC_compute(float analog, uint32_t R, uint16_t B, uint8_t t = 25, uint32_t Rt = 10000, uint8_t res = 10, double adc_vcc = 3.3);

// --- Класс GyverNTC для удобного использования термистора ---
class GyverNTC {
   public:
    // Подключение термистора: GND --- Rt --- ADC --- R --- VCC

    GyverNTC() {}

    GyverNTC(uint8_t pin, uint32_t R, uint16_t B, uint8_t t = 25, uint32_t Rt = 10000, uint8_t res = 10, double adc_vcc = 3.3) {
        config(R, B, t, Rt);
        setPin(pin, res, adc_vcc); // Передаем adc_vcc
    }

    void config(uint32_t R, uint16_t B, uint8_t t = 25, uint32_t Rt = 10000) {
        _beta = B;
        _tempBase = t;
        _baseDivRes = (float)R / Rt;
    }

    void setPin(uint8_t pin, uint8_t res = 10, double adc_vcc = 3.3) {
        _pin = pin;
        _res = res;
        _adc_vcc = adc_vcc; // Сохраняем adc_vcc
    }

    float getTemp() {
        return computeTemp(analogRead(_pin), _res);
    }

    float getTempAverage(uint8_t samples = 20) {
        uint16_t aver = 0;
        for (uint8_t i = 0; i < samples; i++) aver += analogRead(_pin);
        return computeTemp((float)aver / samples, _res);
    }

    float computeTemp(float analog, uint8_t res = 10) {
        // Вызываем NTC_computeRR, передавая _adc_vcc
        return NTC_computeRR(analog, _baseDivRes, _beta, _tempBase, res, _adc_vcc);
    }

   private:
    uint8_t _res = 10;
    uint8_t _pin = 0;
    uint16_t _beta = 3435;
    uint8_t _tempBase = 25;
    float _baseDivRes = 1.0;
    double _adc_vcc = 3.3; // Новая приватная переменная для хранения опорного напряжения АЦП
};
#endif