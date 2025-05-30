#include "thermistor.h"

THERMISTOR thermistor(exhaust_temp_pin,        // Analog pin
                      50000,          // Nominal resistance at 25 ºC
                      3950,           // thermistor's beta coefficient
                      32000);         // Value of the series resistor

float get_wabasto_temp(int temp_pin, int exhaust) { // read a sensor value, smoothen it a bit and convert it to C degrees
    return thermistor.read()/10;
}
