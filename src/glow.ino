// Глобальная переменная для максимальной яркости свечи накаливания.
// Это extern переменная, ее значение должно быть установлено из настроек (settings.glow_brightness)
// в основном файле программы (например, low_smoke.ino).
extern unsigned long glow_brightness_max; 

// Глобальная переменная для времени розжига свечи накаливания (в миллисекундах)
extern unsigned long glow_fade_in_duration_ms; 
// Глобальная переменная для времени затухания свечи накаливания (в миллисекундах)
extern unsigned long glow_fade_out_duration_ms; 

// Отладочный флаг для отображения состояния свечи на дисплее (если используется)
extern bool debug_glow_plug_on;

// Объявление extern для переменной glow_left, если она используется в других файлах
extern int glow_left;

// Добавлена статическая переменная для отслеживания предыдущего состояния активности свечи
static bool prev_glow_active_state = false;


void glow_plug() {
    // Эта функция управляет работой свечи накаливания.
    // Розжиг свечи в течение glow_fade_in_duration_ms
    // Затухание свечи в течение glow_fade_out_duration_ms
    // Если время накала (glow_time) больше 0, свеча включается, и время накала уменьшается каждую секунду.

    static long timer; // Статическая переменная для хранения времени таймера
    static bool is_fading_out = false; // Флаг для отслеживания процесса плавного выключения
    static unsigned long fade_out_start = 0; // Время начала плавного выключения
    static unsigned long glow_start_time = 0; // Время начала текущего цикла накала

    // Если оставшееся время накала меньше 0, сбрасываем время накала
    if(glow_left < 0) {
        glow_time = 0; 
    }

    // Определяем текущее желаемое состояние свечи (включена или выключена)
    bool current_glow_active_state = (glow_time > 0);

    // Обнаружение "восходящего фронта" - свеча только что была активирована
    if (current_glow_active_state && !prev_glow_active_state) {
        // Свеча только что перешла из выключенного состояния во включенное
        glow_start_time = millis(); // Сбрасываем время начала розжига
        is_fading_out = false;      // Отменяем любой текущий процесс затухания
    }

    // Логика плавного включения свечи
    if (current_glow_active_state) {
        unsigned long current_time = millis();
        unsigned long fade_time = current_time - glow_start_time;
        
        if (fade_time < glow_fade_in_duration_ms) {
            // Плавное увеличение яркости в течение glow_fade_in_duration_ms
            // Используем glow_brightness_max, которое теперь будет загружаться из настроек
            int brightness = map(fade_time, 0, glow_fade_in_duration_ms, 0, glow_brightness_max);
            analogWrite(glow_plug_pin, brightness);
            debug_glow_plug_on = true; // Устанавливаем отладочный флаг
        } else {
            // Свеча полностью включена
            analogWrite(glow_plug_pin, glow_brightness_max); // Используем glow_brightness_max из настроек
            debug_glow_plug_on = true; // Устанавливаем отладочный флаг, что свеча горит
            is_fading_out = false; // Сброс флага плавного выключения, так как свеча ещё горит
        }
    } else {
        // Если свеча должна быть выключена (glow_time == 0)
        if (debug_glow_plug_on && !is_fading_out) { 
            // Если свеча горела и ещё не начато плавное выключение
            is_fading_out = true;
            fade_out_start = millis();
        }

        if (is_fading_out) {
            unsigned long fade_time = millis() - fade_out_start;
            
            if (fade_time < glow_fade_out_duration_ms) {
                // Плавное уменьшение яркости в течение glow_fade_out_duration_ms
                // Используем glow_brightness_max, которое теперь будет загружаться из настроек
                int brightness = map(fade_time, 0, glow_fade_out_duration_ms, glow_brightness_max, 0);
                analogWrite(glow_plug_pin, brightness);
            } else {
                // Завершение плавного выключения
                analogWrite(glow_plug_pin, 0); // Полностью выключаем свечу
                debug_glow_plug_on = false; // Сбрасываем отладочный флаг
                is_fading_out = false; // Сбрасываем флаг плавного выключения
            }
        } else {
            // Если свеча должна быть выключена и не находится в процессе затухания
            analogWrite(glow_plug_pin, 0);
            debug_glow_plug_on = false;
        }
    }

    // Обновляем предыдущее состояние для следующего цикла
    prev_glow_active_state = current_glow_active_state;
}

