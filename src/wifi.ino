#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h> // Библиотека для работы с JSON
#include <ESP8266mDNS.h> // Библиотека для mDNS
#include <WiFiManager.h> // Добавлена библиотека WiFiManager


// Имя хоста для mDNS (например, http://espwebasto.local в браузере)
const char* mdns_hostname = "espwebasto";

// Объект веб-сервера на порту 80
ESP8266WebServer server(80);
// Объект WebSocket сервера на порту 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Объявление внешних переменных из других файлов .ino
// Эти переменные должны быть определены в вашем основном коде (например, low_smoke.ino)
extern float exhaust_temp;
extern float fan_speed;
extern float fuel_need; // Эта переменная теперь может быть не нужна, если fuel_need рассчитывается из delayed_period
extern int glow_time;
extern int glow_left; // Добавлено extern для glow_left
extern int burn_mode;
extern bool burn;
extern bool webasto_fail;
extern const char* message;
extern int attempt;
extern int delayed_period; // Добавлено объявление extern для delayed_period
extern bool fuelPumpingActive; // Добавлено объявление extern для fuelPumpingActive

extern SystemState currentState;

extern Settings settings;

// Новые extern объявления для глобальных переменных из fan.txt и glow.txt
extern int max_pwm_global;
extern unsigned long glow_brightness_max; // Изменено на unsigned long
extern unsigned long glow_fade_in_duration_ms; // Изменено на unsigned long
extern unsigned long glow_fade_out_duration_ms; // Изменено на unsigned long


// Объявление внешних функций для управления
// Эти функции должны быть определены в вашем основном коде (например, control.ino или low_smoke.ino)
extern void handleUpCommand();
extern void handleDownCommand();
extern void handleEnterCommand();
extern void handleFuelPumpingCommand();
// ОБНОВЛЕНО: Добавлен параметр bool is_from_websocket к объявлению extern
extern void handleSettingsUpdate(char* paramsStr, bool is_from_websocket); 
extern void resetToDefaultSettings(); // Добавлено для сброса настроек через веб-интерфейс

// Переменная для отслеживания, подключен ли клиент к WebSocket
bool wsConnected = false;

// HTML-страница для веб-интерфейса (ВОССТАНОВЛЕНА ПОЛНАЯ ВЕРСИЯ)
// Использование const char PROGMEM для хранения больших строк во флеш-памяти
const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Управление Webasto</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
    <style>
        body {
            font-family: 'Inter', sans-serif;
            background-color: #1a202c; /* Темный фон */
            color: #e2e8f0; /* Светлый текст */
        }
        .card {
            background-color: #2d3748; /* Темно-серый фон для карточек */
            border-radius: 0.75rem; /* Закругленные углы */
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
        }
        .btn {
            padding: 0.75rem 1.5rem;
            border-radius: 0.5rem;
            font-weight: 600;
            transition: all 0.2s ease-in-out;
            cursor: pointer;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            /* Добавлены стили для объемности и эффекта нажатия */
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3), 0 1px 3px rgba(0, 0, 0, 0.15);
            transform: translateY(0);
            border: none; /* Убедимся, что нет стандартных границ */
        }
        .btn:hover {
            box-shadow: 0 6px 10px rgba(0, 0, 0, 0.4), 0 2px 5px rgba(0, 0, 0, 0.2);
        }
        .btn:active {
            box-shadow: 0 1px 2px rgba(0, 0, 0, 0.2), 0 1px 1px rgba(0, 0, 0, 0.1);
            transform: translateY(2px);
        }
        .btn-primary {
            background-color: #4299e1; /* Синий */
            background-image: linear-gradient(to bottom right, #4299e1, #3182ce); /* Градиент для объемности */
            color: white;
        }
        .btn-primary:hover {
            background-color: #3182ce;
            background-image: linear-gradient(to bottom right, #3182ce, #2c5282);
        }
        .btn-danger {
            background-color: #e53e3e; /* Красный */
            background-image: linear-gradient(to bottom right, #e53e3e, #c53030); /* Градиент для объемности */
            color: white;
        }
        .btn-danger:hover {
            background-color: #c53030;
            background-image: linear-gradient(to bottom right, #c53030, #9b2c2c);
        }
        .btn-secondary {
            background-color: #4a5568; /* Серый */
            background-image: linear-gradient(to bottom right, #4a5568, #2d3748); /* Градиент для объемности */
            color: white;
        }
        .btn-secondary:hover {
            background-color: #2d3748;
            background-image: linear-gradient(to bottom right, #2d3748, #1a202c);
        }
        .input-field {
            background-color: #4a5568;
            border: 1px solid #6b7280;
            border-radius: 0.5rem;
            padding: 0.5rem 0.75rem;
            color: #e2e8f0;
            width: 100%;
        }
        .slider {
            -webkit-appearance: none;
            width: 100%;
            height: 8px;
            border-radius: 5px;
            background: #6b7280;
            outline: none;
            opacity: 0.7;
            -webkit-transition: .2s;
            transition: opacity .2s;
        }
        .slider:hover {
            opacity: 1;
        }
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4299e1;
            cursor: pointer;
        }
        .slider::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4299e1;
            cursor: pointer;
        }
        .status-indicator {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            display: inline-block;
            margin-left: 8px;
        }
        .status-on { background-color: #48bb78; } /* Зеленый */
        .status-off { background-color: #e53e3e; } /* Красный */
        .status-warn { background-color: #ecc94b; } /* Желтый */
        .glow-icon {
            width: 24px;
            height: 24px;
            vertical-align: middle;
            margin-left: 8px;
        }
        .glow-icon.on { fill: #FFD700; } /* Золотой цвет для включенной свечи */
        .glow-icon.off { fill: #6B7280; } /* Серый цвет для выключенной свечи */
    </style>
</head>
<body class="p-4">
    <div class="max-w-4xl mx-auto space-y-6">
        <h1 class="text-3xl font-bold text-center text-blue-400 mb-8">Управление Webasto</h1>

        <div class="card p-6 grid grid-cols-1 md:grid-cols-2 gap-4">
            <div>
                <h2 class="text-xl font-semibold mb-3">Текущий статус</h2>
                <p class="text-sm text-gray-400 mb-2">Обновляется каждые 1 секунду</p>
                <div class="flex items-center text-lg mb-2">
                    Состояние: <span id="statusMessage" class="ml-2 font-bold">Ожидание...</span>
                    <span id="burnStatusIndicator" class="status-indicator status-off"></span>
                </div>
                <div class="text-lg mb-2">Температура выхлопа: <span id="exhaustTemp" class="font-bold">--</span> &deg;C</div>
                <div class="text-lg mb-2">Скорость вентилятора: <span id="fanSpeed" class="font-bold">--</span> %</div>
                <div class="text-lg mb-2">Расход топлива: <span id="fuelRateHz" class="font-bold">--</span> Гц</div>
                <div class="flex items-center text-lg mb-2">
                    Свеча: 
                    <svg id="glowPlugIcon" class="glow-icon off" viewBox="0 0 24 24" fill="currentColor" stroke="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                        <path d="M12 2L9 11H3L13 22L15 13H21L11 2Z" />
                    </svg>
                </div>
                <div class="text-lg mb-2">Режим горения: <span id="burnMode" class="font-bold">--</span></div>
                <div class="text-lg mb-2">Попытка запуска: <span id="attempt" class="font-bold">--</span></div>
                <div class="text-lg mb-2">Ошибки: <span id="webastoFail" class="font-bold">Нет</span></div>
                <div class="text-lg mb-2">Текущий режим: <span id="currentState" class="font-bold">--</span></div>
                <div class="text-lg mb-2">Прокачка топлива: <span id="fuelPumpingActive" class="font-bold">Нет</span></div>
            </div>
            <div class="flex flex-col space-y-4">
                <h2 class="text-xl font-semibold mb-3">Управление</h2>
                <button id="toggleBurnBtn" class="btn btn-primary">Включить / Выключить</button>
                <div class="grid grid-cols-2 gap-4">
                    <button id="upBtn" class="btn btn-secondary">Вниз (Режим)</button>
                    <button id="downBtn" class="btn btn-secondary">Вверх (Режим)</button>
                </div>
                <button id="fuelPumpBtn" class="btn btn-secondary">Прокачка топлива</button>
                <button id="clearFailBtn" class="btn btn-danger">Сбросить ошибку</button>
            </div>
        </div>

        <div class="card p-6">
            <h2 class="text-xl font-semibold mb-4">Настройки</h2>
            <div class="space-y-4">
                <div>
                    <label for="pumpSize" class="block text-sm font-medium text-gray-400">Размер насоса:</label>
                    <input type="number" id="pumpSize" class="input-field mt-1" min="10" max="100">
                </div>
                <div>
                    <label for="heaterTarget" class="block text-sm font-medium text-gray-400">Целевая температура нагревателя (&deg;C):</label>
                    <input type="range" id="heaterTarget" class="slider mt-1" min="150" max="250" step="1">
                    <span id="heaterTargetValue" class="text-blue-300 font-bold ml-2"></span>
                </div>
                <div>
                    <label for="heaterMin" class="block text-sm font-medium text-gray-400">Минимальная температура нагревателя (&deg;C):</label>
                    <input type="range" id="heaterMin" class="slider mt-1" min="140" max="240" step="1">
                    <span id="heaterMinValue" class="text-blue-300 font-bold ml-2"></span>
                </div>
                <div>
                    <label for="heaterOverheat" class="block text-sm font-medium text-gray-400">Температура перегрева (&deg;C):</label>
                    <input type="range" id="heaterOverheat" class="slider mt-1" min="200" max="300" step="1">
                    <span id="heaterOverheatValue" class="text-blue-300 font-bold ml-2"></span>
                </div>
                <div>
                    <label for="heaterWarning" class="block text-sm font-medium text-gray-400">Температура предупреждения (&deg;C):</label>
                    <input type="range" id="heaterWarning" class="slider mt-1" min="180" max="280" step="1">
                    <span id="heaterWarningValue" class="text-blue-300 font-bold ml-2"></span>
                </div>
                <div>
                    <label for="maxPwmFan" class="block text-sm font-medium text-gray-400">Макс. ШИМ вентилятора (0-255): <span id="maxPwmFanPercent" class="text-blue-300 font-bold"></span>%</label>
                    <input type="number" id="maxPwmFan" class="input-field mt-1" min="0" max="255">
                </div>
                <div>
                    <label for="glowBrightness" class="block text-sm font-medium text-gray-400">Яркость свечи накаливания (0-255): <span id="glowBrightnessPercent" class="text-blue-300 font-bold"></span>%</label>
                    <input type="number" id="glowBrightness" class="input-field mt-1" min="0" max="255">
                </div>
                <div>
                    <label for="glowFadeInDuration" class="block text-sm font-medium text-gray-400">Время розжига свечи (мс):</label>
                    <input type="number" id="glowFadeInDuration" class="input-field mt-1" min="0" max="60000">
                </div>
                <div>
                    <label for="glowFadeOutDuration" class="block text-sm font-medium text-gray-400">Время затухания свечи (мс):</label>
                    <input type="number" id="glowFadeOutDuration" class="input-field mt-1" min="0" max="60000">
                </div>
                <div class="grid grid-cols-2 gap-4">
                    <button id="saveSettingsBtn" class="btn btn-primary">Сохранить настройки</button>
                    <button id="resetSettingsBtn" class="btn btn-danger">Сбросить настройки</button>
                    <button id="loadSettingsBtn" class="btn btn-secondary col-span-2">Загрузить настройки</button> </div>
            </div>
        </div>

        <!-- Консоль отладки скрыта -->
        <!--
        <div class="card p-6">
            <h2 class="text-xl font-semibold mb-4">Консоль отладки</h2>
            <div id="debugConsole" class="bg-gray-800 p-3 rounded-md text-sm h-48 overflow-y-scroll break-words">
                Подключение к устройству...
            </div>
        </div>
        -->
    </div>

    <script>
        var ws;
        // const debugConsole = document.getElementById('debugConsole'); // Закомментировано

        function log(message) {
            // Если консоль отладки скрыта, логирование происходит только в Serial
            // if (debugConsole) {
            //     const p = document.createElement('p');
            //     p.textContent = message;
            //     debugConsole.appendChild(p);
            //     debugConsole.scrollTop = debugConsole.scrollHeight; // Прокрутка вниз
            // }
            console.log(message); // Добавлено для логирования в консоль браузера
        }

        function connectWebSocket() {
            const wsUrl = 'ws://' + window.location.hostname + ':81/';
            
            ws = new WebSocket(wsUrl);
            log('Попытка подключения к WebSocket по адресу: ' + wsUrl);

            ws.onopen = function() {
                log('Подключено к WebSocket.');
                // При открытии соединения, запросим настройки, но не будем обновлять статус
                ws.send('GET_SETTINGS'); 
            };

            ws.onmessage = function(event) {
                log('Получено: ' + event.data);
                try {
                    const data = JSON.parse(event.data);
                    // Проверяем, содержит ли ответ настройки (приходит только по запросу GET_SETTINGS/RESET_SETTINGS)
                    if (data.settings) {
                        applySettingsToForm(data.settings); 
                    } else { // Если это не настройки, значит это обновление статуса
                        updateUI(data); // Обновляем только показатели статуса
                    }
                } catch (e) {
                    log('Ошибка парсинга JSON: ' + e.message + ' Данные: ' + event.data);
                    // Если это не JSON, возможно, это старый формат ответа через Serial
                    if (event.data.startsWith("CURRENT_SETTINGS:")) {
                        parseAndApplySettings(event.data); 
                    }
                }
            };

            ws.onclose = function() {
                log('Отключено от WebSocket. Попытка переподключения через 3 секунды...');
                setTimeout(connectWebSocket, 3000);
            };

            ws.onerror = function(error) {
                log('Ошибка WebSocket: ' + error.message);
            };
        }

        function updateUI(data) {
            // Обновляем только отображаемые значения статуса
            document.getElementById('exhaustTemp').textContent = data.exhaust_temp !== undefined ? data.exhaust_temp.toFixed(1) : '--';
            document.getElementById('fanSpeed').textContent = data.fan_speed !== undefined ? data.fan_speed.toFixed(0) : '--';
            // Обновлено: Расход топлива теперь берется из fuel_rate_hz
            document.getElementById('fuelRateHz').textContent = data.fuel_rate_hz !== undefined ? data.fuel_rate_hz.toFixed(2) : '--';
            
            // Обновлено: Управление иконкой свечи накаливания
            const glowPlugIcon = document.getElementById('glowPlugIcon');
            if (data.debug_glow_plug_on !== undefined) {
                if (data.debug_glow_plug_on) {
                    glowPlugIcon.classList.remove('off');
                    glowPlugIcon.classList.add('on');
                } else {
                    glowPlugIcon.classList.remove('on');
                    glowPlugIcon.classList.add('off');
                }
            }


            document.getElementById('burnMode').textContent = data.burn_mode !== undefined ? data.burn_mode : '--';
            document.getElementById('attempt').textContent = data.attempt !== undefined ? data.attempt : '--';
            document.getElementById('statusMessage').textContent = data.message || 'Неизвестно';
            document.getElementById('fuelPumpingActive').textContent = data.fuel_pumping_active ? 'Активна' : 'Нет';


            const burnStatusIndicator = document.getElementById('burnStatusIndicator');
            if (data.burn !== undefined) {
                if (data.burn) {
                    burnStatusIndicator.className = 'status-indicator status-on';
                } else {
                    burnStatusIndicator.className = 'status-indicator status-off';
                }
            }

            const webastoFailElement = document.getElementById('webastoFail');
            if (data.webasto_fail !== undefined) {
                if (data.webasto_fail) {
                    webastoFailElement.textContent = 'Да';
                    webastoFailElement.classList.add('text-red-500');
                    webastoFailElement.classList.remove('text-green-500');
                } else {
                    webastoFailElement.textContent = 'Нет';
                    webastoFailElement.classList.add('text-green-500');
                    webastoFailElement.classList.remove('text-red-500');
                }
            }

            const currentStateElement = document.getElementById('currentState');
            if (data.currentState !== undefined) {
                let stateText = 'Неизвестно';
                switch(data.currentState) {
                    case 0: stateText = 'HIGH'; break;
                    case 1: stateText = 'MID'; break;
                    case 2: stateText = 'LOW'; break;
                }
                currentStateElement.textContent = stateText;
            }
        }

        // НОВАЯ ФУНКЦИЯ: Применяет настройки к полям формы
        function applySettingsToForm(settingsData) {
            if (settingsData) {
                document.getElementById('pumpSize').value = settingsData.pump_size;
                document.getElementById('heaterTarget').value = settingsData.heater_target;
                document.getElementById('heaterTargetValue').textContent = settingsData.heater_target;
                document.getElementById('heaterMin').value = settingsData.heater_min;
                document.getElementById('heaterMinValue').textContent = settingsData.heater_min;
                document.getElementById('heaterOverheat').value = settingsData.heater_overheat;
                document.getElementById('heaterOverheatValue').textContent = settingsData.heater_overheat;
                document.getElementById('heaterWarning').value = settingsData.heater_warning;
                document.getElementById('heaterWarningValue').textContent = settingsData.heater_warning;
                // Новые настройки
                document.getElementById('maxPwmFan').value = settingsData.max_pwm_fan;
                // Обновлено: Отображение процента для maxPwmFan
                document.getElementById('maxPwmFanPercent').textContent = ((settingsData.max_pwm_fan / 255.0) * 100).toFixed(0);

                document.getElementById('glowBrightness').value = settingsData.glow_brightness;
                // Обновлено: Отображение процента для glowBrightness
                document.getElementById('glowBrightnessPercent').textContent = ((settingsData.glow_brightness / 255.0) * 100).toFixed(0);

                document.getElementById('glowFadeInDuration').value = settingsData.glow_fade_in_duration;
                document.getElementById('glowFadeOutDuration').value = settingsData.glow_fade_out_duration;
            }
        }

        function parseAndApplySettings(dataString) {
            // Ожидаемый формат: "CURRENT_SETTINGS:pump_size=22,heater_target=195,..."
            const parts = dataString.split(':');
            if (parts.length < 2 || parts[0] !== "CURRENT_SETTINGS") {
                log("Неверный формат строки настроек: " + dataString);
                return;
            }
            const params = parts[1].split(',');
            const settings = {};
            params.forEach(param => {
                const [key, value] = param.split('=');
                if (key && value) {
                    settings[key.trim()] = parseInt(value.trim());
                }
            });
            // Вызываем новую функцию для применения настроек к форме
            applySettingsToForm(settings); 
        }


        // Обработчики событий для кнопок
        document.getElementById('toggleBurnBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('ENTER');
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('upBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('UP');
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('downBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('DOWN');
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('fuelPumpBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('FP');
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('clearFailBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('CF');
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('saveSettingsBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                const pumpSize = document.getElementById('pumpSize').value;
                const heaterTarget = document.getElementById('heaterTarget').value;
                const heaterMin = document.getElementById('heaterMin').value;
                const heaterOverheat = document.getElementById('heaterOverheat').value;
                const heaterWarning = document.getElementById('heaterWarning').value;
                // Новые настройки
                const maxPwmFan = document.getElementById('maxPwmFan').value;
                const glowBrightness = document.getElementById('glowBrightness').value;
                const glowFadeInDuration = document.getElementById('glowFadeInDuration').value;
                const glowFadeOutDuration = document.getElementById('glowFadeOutDuration').value;


                const command = `SET:pump_size=${pumpSize},heater_target=${heaterTarget},heater_min=${heaterMin},heater_overheat=${heaterOverheat},heater_warning=${heaterWarning},max_pwm_fan=${maxPwmFan},glow_brightness=${glowBrightness},glow_fade_in_duration=${glowFadeInDuration},glow_fade_out_duration=${glowFadeOutDuration}`;
                ws.send(command);
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('resetSettingsBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('RESET_SETTINGS');
            } else {
                log('WebSocket не подключен.');
            }
        });

        // НОВАЯ КНОПКА: Загрузить настройки
        document.getElementById('loadSettingsBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('GET_SETTINGS'); // Отправляем команду для получения текущих настроек
                log('Запрос текущих настроек...');
            } else {
                log('WebSocket не подключен.');
            }
        });

        // Обновление значений слайдеров
        document.getElementById('heaterTarget').addEventListener('input', function() {
            document.getElementById('heaterTargetValue').textContent = this.value;
        });
        document.getElementById('heaterMin').addEventListener('input', function() {
            document.getElementById('heaterMinValue').textContent = this.value;
        });
        document.getElementById('heaterOverheat').addEventListener('input', function() {
            document.getElementById('heaterOverheatValue').textContent = this.value;
        });
        document.getElementById('heaterWarning').addEventListener('input', function() {
            document.getElementById('heaterWarningValue').textContent = this.value;
        });


        // Инициализация подключения при загрузке страницы
        window.onload = connectWebSocket;

    </script>
</body>
</html>
)rawliteral";

// Функция для инициализации Wi-Fi в режиме клиента
void setup_wifi_station() {
  Serial.println();
  Serial.println("DEBUG: === Starting WiFi Station setup ===");
  Serial.flush();

  // Создаем объект WiFiManager
  WiFiManager wifiManager;

  // Устанавливаем режим сна Wi-Fi в WIFI_NONE_SLEEP для повышения стабильности
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Устанавливаем колбэк для портала конфигурации (чтобы видеть, когда он активен)
  wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial.println("DEBUG: Entered WiFi setup portal mode.");
    Serial.print("DEBUG: Connect to AP: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());
    Serial.print("DEBUG: IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.flush();
  });

  // autoConnect() будет пытаться подключиться к сохраненным учетным данным.
  // Если не удастся, он запустит Captive Portal.
  // Имя AP для Captive Portal будет "AutoConnectAP" или указанное вами.
  // В данном случае, мы хотим использовать mdns_hostname как имя AP для удобства.
  // Если autoConnect() возвращает false, это значит, что портал был запущен,
  // но пользователь не настроил Wi-Fi или произошла ошибка.
  if (!wifiManager.autoConnect(mdns_hostname)) { // Используем mdns_hostname как SSID для AP
    Serial.println("ERROR: Failed to connect to WiFi or configure via portal. Continuing without network services.");
    Serial.flush();
    // Здесь функция просто завершается, позволяя setup() и loop() продолжить работу.
    // Веб-сервер и WebSocket не будут запущены.
    return; 
  }

  // Если мы дошли сюда, значит, подключение к Wi-Fi успешно или пользователь настроил его.
  Serial.println("\nDEBUG: WiFi connected successfully.");
  Serial.print("DEBUG: IP address: ");
  Serial.println(WiFi.localIP());
  Serial.flush();

  // Инициализация mDNS
  if (MDNS.begin(mdns_hostname)) {
    Serial.print("DEBUG: mDNS responder started. Access at http://");
    Serial.print(mdns_hostname);
    Serial.println(".local/");
    Serial.flush();
  } else {
    Serial.println("ERROR: mDNS setup failed!");
    Serial.flush();
  }

  Serial.println("DEBUG: Setting up HTTP server route for '/'..."); 
  Serial.flush();
  // Обработчик корневого URL
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML); // Использование send_P с PROGMEM
    Serial.println("DEBUG: HTTP request for '/' received and page sent.");
    Serial.flush();
  });
  Serial.println("DEBUG: HTTP server route for '/' configured."); 
  Serial.flush();

  Serial.println("DEBUG: Starting HTTP server...");
  Serial.flush();
  server.begin();
  Serial.println("DEBUG: HTTP server started.");
  Serial.flush();

  Serial.println("DEBUG: Starting WebSocket server...");
  Serial.flush();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("DEBUG: WebSocket server started.");
  Serial.println("DEBUG: === WiFi Station setup complete ===");
  Serial.flush();
}

// Функция для обработки клиентов Wi-Fi и WebSocket
void handle_wifi_clients() {
  // Проверяем, подключен ли Wi-Fi, прежде чем пытаться обрабатывать клиентов
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    webSocket.loop(); // ОЧЕНЬ ВАЖНО: Вызывать webSocket.loop() как можно чаще в loop()
    MDNS.update(); // Обязательно вызывайте в loop() для работы mDNS
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      wsConnected = false;
      break;
    case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        wsConnected = true;
        // Отправляем текущие настройки клиенту сразу после подключения
        sendCurrentSettings();
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
      // Обработка текстовых команд от клиента WebSocket
      if (strcmp((char*)payload, "GET_SETTINGS") == 0) {
        sendCurrentSettings();
      } else if (strcmp((char*)payload, "RESET_SETTINGS") == 0) {
        // Вызов функции сброса настроек из low_smoke.ino
        resetToDefaultSettings(); 
        sendCurrentSettings(); // Отправляем обновленные настройки
      } else if (strncmp((char*)payload, "SET:", 4) == 0) {
        char cleanedCommand[256]; // Достаточно большой буфер
        strncpy(cleanedCommand, (char*)payload, sizeof(cleanedCommand) - 1);
        cleanedCommand[sizeof(cleanedCommand) - 1] = '\0'; // Гарантируем нулевой терминатор

        // При вызове из WebSocket, is_from_websocket = true
        handleSettingsUpdate(cleanedCommand + 4, true); 
      } else if (strcmp((char*)payload, "UP") == 0) { 
        handleUpCommand();
      } else if (strcmp((char*)payload, "DOWN") == 0) { 
        handleDownCommand();
      } else if (strcmp((char*)payload, "ENTER") == 0) { 
        handleEnterCommand();
      } else if (strcmp((char*)payload, "FP") == 0) { 
        handleFuelPumpingCommand();
      } else if (strcmp((char*)payload, "CF") == 0) { 
        webasto_fail = false; // Сброс флага ошибки
      }
      break;
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    case WStype_PING: 
    case WStype_PONG: 
      break;
    default:
      Serial.printf("[%u] Unhandled WebSocket Event Type: %d\n", num, type);
      break;
  }
}

// Функция для отправки данных о состоянии по WebSocket
void send_status_update() {
  if (wsConnected) {
    StaticJsonDocument<256> doc; 

    // Расчет "Расход топлива" (fuel_rate_hz)
    float calculated_fuel_rate_hz = 0.0;
    if (delayed_period > 0) {
      calculated_fuel_rate_hz = 1000.00 / delayed_period;
    }
    
    // Отправка данных о состоянии (на корневом уровне JSON)
    doc["exhaust_temp"] = exhaust_temp;
    doc["fan_speed"] = fan_speed;
    doc["fuel_rate_hz"] = calculated_fuel_rate_hz; 
    doc["glow_time"] = glow_time; 
    doc["burn_mode"] = burn_mode;
    doc["webasto_fail"] = webasto_fail;
    doc["debug_glow_plug_on"] = debug_glow_plug_on;
    doc["fuel_pumping_active"] = fuelPumpingActive;
    doc["message"] = message; 
    doc["attempt"] = attempt; 
    doc["burn"] = burn; 
    doc["currentState"] = currentState; 


    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
    Serial.println("DEBUG: Sent status update via WebSocket."); 
  } else {
    Serial.println("DEBUG: WebSocket not connected, status update skipped."); 
  }
}

// Переопределение sendCurrentSettings для отправки через WebSocket
// Эта функция будет вызываться из основного кода, но фактически отправлять данные через WebSocket
void sendCurrentSettings() {
  if (wsConnected) {
    StaticJsonDocument<256> doc; 

    // Важно: настройки отправляются внутри объекта "settings"
    doc["settings"]["pump_size"] = settings.pump_size;
    doc["settings"]["heater_target"] = settings.heater_target;
    doc["settings"]["heater_min"] = settings.heater_min;
    doc["settings"]["heater_overheat"] = settings.heater_overheat;
    doc["settings"]["heater_warning"] = settings.heater_warning;
    doc["settings"]["max_pwm_fan"] = settings.max_pwm_fan;
    doc["settings"]["glow_brightness"] = settings.glow_brightness;
    doc["settings"]["glow_fade_in_duration"] = settings.glow_fade_in_duration;
    doc["settings"]["glow_fade_out_duration"] = settings.glow_fade_out_duration;


    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
    Serial.println("DEBUG: Sent settings via WebSocket."); 
  } else {
    // Если WebSocket не подключен, можно отправить через Serial как раньше
    Serial.print(F("CURRENT_SETTINGS:"));
    Serial.print(F("pump_size=")); Serial.print(settings.pump_size); Serial.print(F(","));
    Serial.print(F("heater_target=")); Serial.print(settings.heater_target); Serial.print(F(","));
    Serial.print(F("heater_min=")); Serial.print(settings.heater_min); Serial.print(F(","));
    Serial.print(F("heater_overheat=")); Serial.print(settings.heater_overheat); Serial.print(F(","));
    Serial.print(F("heater_warning=")); Serial.print(settings.heater_warning); Serial.print(F(","));
    // Новые настройки
    Serial.print(F("max_pwm_fan=")); Serial.print(settings.max_pwm_fan); Serial.print(F(","));
    Serial.print(F("glow_brightness=")); Serial.print(settings.glow_brightness); Serial.print(F(","));
    Serial.print(F("glow_fade_in_duration=")); Serial.print(settings.glow_fade_in_duration); Serial.print(F(","));
    Serial.print(F("glow_fade_out_duration=")); Serial.println(settings.glow_fade_out_duration);
    Serial.println(F("DEBUG: Sent settings via Serial (WebSocket not connected).")); 
  }
}
