#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h> // Библиотека для работы с JSON
#include <ESP8266mDNS.h> // Библиотека для mDNS
#include <WiFiManager.h> // Добавлена библиотека WiFiManager

// Глобальная переменная для управления логированием
extern bool loggingEnabled;

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


// Новые глобальные переменные для учета расхода топлива
extern float total_fuel_consumed_liters; // Общее количество потребленного топлива в литрах
extern float fuel_consumption_per_hour;  // Средний расход топлива за 1 час в литрах/час

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

        /* Стили для вкладок (обновлено для вида "закладок") */
        .tab-buttons {
            display: flex;
            justify-content: flex-start; /* Выравнивание вкладок по левому краю */
            margin-bottom: -5; /* Убираем отступ, чтобы вкладки прилегали к контенту */
            overflow-x: auto; /* Добавляем горизонтальную прокрутку */
            -webkit-overflow-scrolling: touch; /* Для плавности на iOS */
            scrollbar-width: none; /* Скрыть полосу прокрутки для Firefox */
            -ms-overflow-style: none;  /* Скрыть полосу прокрутки для IE/Edge */
        }
        .tab-buttons::-webkit-scrollbar { /* Скрыть полосу прокрутки для Chrome/Safari */
            display: none;
        }
        .tab-button {
            flex-shrink: 0; /* Важно, чтобы вкладки не сжимались */
            padding: 0.4rem 0.8rem; /* Уменьшаем padding для меньшего размера */
            text-align: center;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease-in-out, transform 0.2s ease-in-out; /* Добавляем transition для transform */
            color: #a0aec0; /* Светло-серый текст для неактивных вкладок */
            background-color: #222b37; /* Темный фон для неактивных вкладок */
            border-top-left-radius: 0.4rem; /* Немного меньше радиус для меньших вкладок */
            border-top-right-radius: 0.4rem; /* Немного меньше радиус для меньших вкладок */
            border: 1px solid #3a4352; /* Более темная граница для неактивных вкладок */
            border-bottom: 1px solid #2d3748; /* Нижняя граница неактивной вкладки совпадает с фоном активной области */
            margin-right: 0.1rem; /* Еще меньший отступ между вкладками */
            position: relative;
            z-index: 0; /* Неактивные вкладки находятся сзади */
            font-size: 0.85rem; /* Немного уменьшаем размер шрифта */
            transform: translateY(3px); /* Опускаем неактивные вкладки */
        }
        .tab-button:hover {
            background-color: #3182ce; /* Эффект при наведении */
            color: white;
            transform: translateY(0); /* Поднимаем при наведении */
            border-bottom-color: #3182ce; /* Граница при наведении */
        }
        .tab-button.active {
            background-color: #2d3748; /* Фон активной вкладки соответствует фону карточки контента */
            color: white;
            border-bottom: none; /* УДАЛЕНО: Чтобы не было никаких видимых границ */
            z-index: 1; /* Активная вкладка находится сверху */
            padding-top: 0.5rem; /* Немного больший отступ сверху для активной вкладки */
            padding-bottom: 0.4rem; /* Сохраняем нижний отступ, чтобы выглядело поднятым */
            margin-bottom: -1px; /* Убираем margin-bottom для активной вкладки, чтобы она прилегала */
            transform: translateY(0); /* Активная вкладка не смещена */
            border-color: #4a5568; /* Стандартная граница для активной вкладки */
            border-left: 1px solid #4a5568; /* Добавлено для сохранения левой границы */
            border-right: 1px solid #4a5568; /* Добавлено для сохранения правой границы */
            border-top: 1px solid #4a5568; /* Добавлено для сохранения верхней границы */
        }
        /* Корректировка верхнего отступа карточки контента для выравнивания с вкладками */
        .tab-content.card {
            margin-top: -5px; /* Убираем верхний отступ, поднимая контент на 1px */
            border-top-left-radius: 0.0rem; /* Закругляем верхний левый угол карточки */
            border-top-right-radius: 0.0rem; /* Закругляем верхний правый угол карточки */
            border-left: 1px solid #4a5568; /* Добавляем общую границу для карточки */
            border-right: 1px solid #4a5568;
            border-bottom: 1px solid #4a5568;
            border-top: none; /* Явно убираем верхнюю границу */
        }
        /* Скрываем все вкладки по умолчанию и добавляем плавный переход */
        .tab-content {
            display: none;
            opacity: 0;
            transition: opacity 0.3s ease-in-out;
        }
        /* Показываем активную вкладку */
        .tab-content.active {
            display: block;
            opacity: 1;
        }
    </style>
</head>
<body class="p-4">
    <div class="max-w-3xl mx-auto space-y-6"> <!-- Уменьшен max-width для уменьшения размера страницы -->
        <h1 class="text-3xl font-bold text-center text-blue-400 mb-8">Управление Webasto</h1>

        <!-- Кнопки вкладок -->
        <div class="tab-buttons">
            <div class="tab-button active" onclick="openTab(event, 'controlStatus')">Управление и статус</div>
            <div class="tab-button" onclick="openTab(event, 'settings')">Настройки</div>
            <div class="tab-button" onclick="openTab(event, 'log')">Лог</div>
            <div class="tab-button" onclick="openTab(event, 'wifiSettings')">Wi-Fi</div>
            <div class="tab-button" onclick="openTab(event, 'fuelConsumption')">Расход топлива</div>
        </div>

        <!-- Содержимое вкладки "Управление и статус" -->
        <div id="controlStatus" class="tab-content active card p-6 grid grid-cols-1 md:grid-cols-2 gap-4">
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

        <!-- Содержимое вкладки "Настройки" -->
        <div id="settings" class="tab-content card p-6">
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

        <!-- Содержимое вкладки "Лог" -->
        <div id="log" class="tab-content card p-6">
            <h2 class="text-xl font-semibold mb-4">Лог устройства</h2>
            <div class="mb-4">
                <input type="checkbox" id="enableLoggingCheckbox" class="mr-2">
                <label for="enableLoggingCheckbox" class="text-lg">Включить лог</label>
            </div>
            <div id="debugConsole" class="bg-gray-800 p-3 rounded-md text-sm h-96 overflow-y-scroll break-words">
                Подключение к устройству...
            </div>
        </div>

        <!-- Содержимое вкладки "Wi-Fi" -->
        <div id="wifiSettings" class="tab-content card p-6">
            <h2 class="text-xl font-semibold mb-4">Настройки Wi-Fi</h2>
            <div class="space-y-4">
                <div class="text-lg mb-2">Статус подключения: <span id="wifiStatus" class="font-bold">--</span></div>
                <div class="text-lg mb-2">SSID: <span id="wifiSSID" class="font-bold">--</span></div>
                <div class="text-lg mb-2">IP Адрес: <span id="wifiIP" class="font-bold">--</span></div>
                <button id="resetWifiBtn" class="btn btn-danger">Сбросить настройки Wi-Fi</button>
                <button id="rebootEspBtn" class="btn btn-secondary">Перезагрузить ESP</button>
            </div>

            <h3 class="text-xl font-semibold mb-3 mt-6">Поиск и подключение к Wi-Fi</h3>
            <button id="scanWifiBtn" class="btn btn-primary mb-4">Найти Wi-Fi сети</button>
            <div id="wifiScanResults" class="space-y-2 mb-4">
                <!-- Результаты сканирования будут здесь -->
            </div>
            <div class="space-y-4">
                <div>
                    <label for="manualSSID" class="block text-sm font-medium text-gray-400">Имя сети (SSID):</label>
                    <input type="text" id="manualSSID" class="input-field mt-1" placeholder="Введите SSID">
                </div>
                <div>
                    <label for="manualPassword" class="block text-sm font-medium text-gray-400">Пароль:</label>
                    <input type="password" id="manualPassword" class="input-field mt-1" placeholder="Введите пароль">
                </div>
                <button id="connectWifiBtn" class="btn btn-primary">Подключиться</button>
            </div>
        </div>

        <!-- Содержимое вкладки "Расход топлива" -->
        <div id="fuelConsumption" class="tab-content card p-6">
            <h2 class="text-xl font-semibold mb-4">Расход топлива</h2>
            <div class="space-y-4">
                <div class="text-lg mb-2">Текущее потребление: <span id="currentFuelConsumed" class="font-bold">--</span> л</div>
                <div class="text-lg mb-2">Расчетный расход за час: <span id="hourlyFuelConsumption" class="font-bold">--</span> л/ч</div>
                <button id="resetFuelBtn" class="btn btn-danger">Сбросить текущее потребление</button>
            </div>
        </div>
    </div>

    <script>
        var ws;
        const debugConsole = document.getElementById('debugConsole'); 

        function log(message) {
            console.log(message); 
            const enableLoggingCheckbox = document.getElementById('enableLoggingCheckbox');
            if (debugConsole && enableLoggingCheckbox && enableLoggingCheckbox.checked) {
                const p = document.createElement('p');
                p.textContent = message;
                debugConsole.appendChild(p);
                debugConsole.scrollTop = debugConsole.scrollHeight; 
            }
        }

        function connectWebSocket() {
            const wsUrl = 'ws://' + window.location.hostname + ':81/';
            
            ws = new WebSocket(wsUrl);
            log('Попытка подключения к WebSocket по адресу: ' + wsUrl);

            ws.onopen = function() {
                log('Подключено к WebSocket.');
                ws.send('GET_SETTINGS'); 
            };

            ws.onmessage = function(event) {
                log('Получено: ' + event.data);
                try {
                    const data = JSON.parse(event.data);
                    if (data.settings) {
                        applySettingsToForm(data.settings); 
                    } else { 
                        updateUI(data); 
                    }
                } catch (e) {
                    log('Ошибка парсинга JSON: ' + e.message + ' Данные: ' + event.data);
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
            document.getElementById('exhaustTemp').textContent = data.exhaust_temp !== undefined ? data.exhaust_temp.toFixed(1) : '--';
            document.getElementById('fanSpeed').textContent = data.fan_speed !== undefined ? data.fan_speed.toFixed(0) : '--';
            document.getElementById('fuelRateHz').textContent = data.fuel_rate_hz !== undefined ? data.fuel_rate_hz.toFixed(2) : '--';
            
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

            // WiFi Status
            const wifiStatusElement = document.getElementById('wifiStatus');
            const wifiSSIDElement = document.getElementById('wifiSSID');
            const wifiIPElement = document.getElementById('wifiIP');

            if (data.wifi_status !== undefined) {
                if (data.wifi_status === 3) { 
                    wifiStatusElement.textContent = 'Подключено';
                    wifiStatusElement.classList.remove('text-red-500', 'text-yellow-500');
                    wifiStatusElement.classList.add('text-green-500');
                    wifiSSIDElement.textContent = data.wifi_ssid || '--';
                    wifiIPElement.textContent = data.wifi_ip || '--';
                } else { 
                    wifiStatusElement.textContent = 'Настройка AP';
                    wifiStatusElement.classList.remove('text-green-500', 'text-red-500');
                    wifiStatusElement.classList.add('text-yellow-500');
                    wifiSSIDElement.textContent = data.wifi_ssid || '--'; 
                    wifiIPElement.textContent = data.wifi_ip || '--';
                }
            }

            // Logging Enabled Status
            const enableLoggingCheckbox = document.getElementById('enableLoggingCheckbox');
            if (data.logging_enabled !== undefined) {
                enableLoggingCheckbox.checked = data.logging_enabled;
            }

            // WiFi Scan Results
            if (data.wifi_networks) {
                const resultsDiv = document.getElementById('wifiScanResults');
                resultsDiv.innerHTML = ''; 
                if (data.wifi_networks.length > 0) {
                    const ul = document.createElement('ul');
                    ul.className = 'list-disc list-inside';
                    data.wifi_networks.forEach(network => {
                        const li = document.createElement('li');
                        li.className = 'text-sm cursor-pointer hover:text-blue-300';
                        li.textContent = `${network.ssid} (RSSI: ${network.rssi} dBm)`;
                        li.addEventListener('click', () => {
                            document.getElementById('manualSSID').value = network.ssid;
                            document.getElementById('manualPassword').value = ''; 
                        });
                        ul.appendChild(li);
                    });
                    resultsDiv.appendChild(ul);
                } else {
                    resultsDiv.innerHTML = '<p class="text-red-300">Сети не найдены.</p>';
                }
            }

            // Fuel Consumption Display
            if (data.total_fuel_consumed_liters !== undefined) {
                document.getElementById('currentFuelConsumed').textContent = data.total_fuel_consumed_liters.toFixed(2);
            } else {
                document.getElementById('currentFuelConsumed').textContent = '--'; // Убедимся, что отображается '--' если undefined
            }
            if (data.fuel_consumption_per_hour !== undefined) {
                document.getElementById('hourlyFuelConsumption').textContent = data.fuel_consumption_per_hour.toFixed(2);
            } else {
                document.getElementById('hourlyFuelConsumption').textContent = '--'; // Убедимся, что отображается '--' если undefined
            }
        }

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

                // Расчет и отображение процента для maxPwmFan
                document.getElementById('maxPwmFanPercent').textContent = ((settingsData.max_pwm_fan / 255.0) * 100).toFixed(0);
                document.getElementById('maxPwmFan').value = settingsData.max_pwm_fan;
                
                document.getElementById('glowBrightness').value = settingsData.glow_brightness;
                document.getElementById('glowBrightnessPercent').textContent = ((settingsData.glow_brightness / 255.0) * 100).toFixed(0);

                document.getElementById('glowFadeInDuration').value = settingsData.glow_fade_in_duration;
                document.getElementById('glowFadeOutDuration').value = settingsData.glow_fade_out_duration;
            }
        }

        function parseAndApplySettings(dataString) {
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
            applySettingsToForm(settings); 
        }

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

        document.getElementById('loadSettingsBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('GET_SETTINGS'); 
                log('Запрос текущих настроек...');
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('resetWifiBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('RESET_WIFI');
                log('Отправлена команда: Сбросить настройки Wi-Fi.');
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('rebootEspBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('REBOOT_ESP');
                log('Отправлена команда: Перезагрузить ESP.');
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('enableLoggingCheckbox').addEventListener('change', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(this.checked ? 'ENABLE_LOGGING' : 'DISABLE_LOGGING');
                log('Отправлена команда: ' + (this.checked ? 'Включить' : 'Выключить') + ' лог.');
            } else {
                log('WebSocket не подключен.');
                this.checked = !this.checked; 
            }
        });

        document.getElementById('scanWifiBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('SCAN_WIFI');
                log('Отправлена команда: Сканировать Wi-Fi сети.');
                document.getElementById('wifiScanResults').innerHTML = '<p class="text-yellow-300">Сканирование...</p>';
            } else {
                log('WebSocket не подключен.');
            }
        });

        document.getElementById('connectWifiBtn').addEventListener('click', function() {
            const ssid = document.getElementById('manualSSID').value;
            const password = document.getElementById('manualPassword').value;
            if (ssid && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(`CONNECT_WIFI:${ssid},${password}`);
                log(`Отправлена команда: Подключиться к Wi-Fi "${ssid}".`);
            } else {
                log('Ошибка: SSID не может быть пустым или WebSocket не подключен.');
            }
        });

        document.getElementById('resetFuelBtn').addEventListener('click', function() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('RESET_FUEL_CONSUMPTION');
                log('Отправлена команда: Сбросить текущее потребление топлива.');
            } else {
                log('WebSocket не подключен.');
            }
        });


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

        function openTab(evt, tabName) {
            var i, tabcontent, tablinks;
            tabcontent = document.getElementsByClassName("tab-content");
            for (i = 0; i < tabcontent.length; i++) {
                tabcontent[i].style.display = "none"; 
                tabcontent[i].classList.remove('active'); 
            }
            tablinks = document.getElementsByClassName("tab-button");
            for (i = 0; i < tablinks.length; i++) {
                tablinks[i].classList.remove("active"); 
            }
            document.getElementById(tabName).style.display = "block"; 
            document.getElementById(tabName).classList.add('active'); 
            evt.currentTarget.classList.add("active"); 
        }

        window.onload = function() {
            connectWebSocket();
            // Скрываем все вкладки, кроме первой, при загрузке страницы
            var tabContents = document.getElementsByClassName("tab-content");
            for (var i = 1; i < tabContents.length; i++) {
                tabContents[i].style.display = "none";
                tabContents[i].classList.remove('active');
            }
            document.getElementById('controlStatus').style.display = 'block';
            document.getElementById('controlStatus').classList.add('active');
            document.querySelector('.tab-buttons .tab-button:first-child').classList.add('active');
        };

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
      } else if (strcmp((char*)payload, "RESET_WIFI") == 0) {
        Serial.println("DEBUG: Received RESET_WIFI command. Resetting WiFi settings.");
        WiFiManager wifiManager;
        wifiManager.resetSettings(); // Очищает сохраненные учетные данные Wi-Fi
        Serial.println("DEBUG: WiFi settings cleared. Rebooting to apply changes.");
        ESP.restart(); // Перезагружаем ESP, чтобы WiFiManager запустил портал
      } else if (strcmp((char*)payload, "REBOOT_ESP") == 0) {
        Serial.println("DEBUG: Received REBOOT_ESP command. Rebooting.");
        ESP.restart(); // Просто перезагружаем ESP
      } else if (strcmp((char*)payload, "ENABLE_LOGGING") == 0) {
          loggingEnabled = true;
          Serial.println("DEBUG: Logging enabled.");
          send_status_update(); // Отправить обновленный статус
      } else if (strcmp((char*)payload, "DISABLE_LOGGING") == 0) {
          loggingEnabled = false;
          Serial.println("DEBUG: Logging disabled.");
          send_status_update(); // Отправить обновленный статус
      } else if (strcmp((char*)payload, "SCAN_WIFI") == 0) {
          Serial.println("DEBUG: Received SCAN_WIFI command. Scanning networks...");
          int n = WiFi.scanNetworks();
          Serial.printf("DEBUG: Scan done. Found %d networks.\n", n);
          StaticJsonDocument<512> doc; // Увеличиваем размер для списка сетей
          JsonArray networksArray = doc.createNestedArray("wifi_networks");
          for (int i = 0; i < n; ++i) {
              JsonObject network = networksArray.createNestedObject();
              network["ssid"] = WiFi.SSID(i);
              network["rssi"] = WiFi.RSSI(i);
          }
          String jsonString;
          serializeJson(doc, jsonString);
          webSocket.broadcastTXT(jsonString);
          WiFi.scanDelete(); // Очищаем результаты сканирования
      } else if (strncmp((char*)payload, "CONNECT_WIFI:", 13) == 0) {
          char* commandStr = (char*)payload + 13;
          char* ssid = strtok(commandStr, ",");
          char* password = strtok(NULL, ",");

          if (ssid) {
              Serial.printf("DEBUG: Received CONNECT_WIFI command. Connecting to SSID: %s\n", ssid);
              WiFi.begin(ssid, password ? password : ""); // Если пароль пустой, передаем пустую строку
              // WiFiManager handles saving credentials automatically once connected
              // No need to call wifiManager.autoConnect() or saveConfig() here.
              // Just try to connect. The UI will get status updates.
          } else {
              Serial.println("ERROR: CONNECT_WIFI command missing SSID.");
          }
      } else if (strcmp((char*)payload, "RESET_FUEL_CONSUMPTION") == 0) { // NEW: Reset fuel consumption
          Serial.println("DEBUG: Received RESET_FUEL_CONSUMPTION command. Resetting total fuel consumed.");
          total_fuel_consumed_liters = 0.0;
          send_status_update(); // Обновляем UI
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
    StaticJsonDocument<512> doc; // Увеличено до 512 байт

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

    // Добавляем данные о Wi-Fi
    doc["wifi_status"] = WiFi.status();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["wifi_ip"] = WiFi.localIP().toString();

    // Добавляем состояние логирования
    doc["logging_enabled"] = loggingEnabled;

    // Добавляем данные о расходе топлива
    doc["total_fuel_consumed_liters"] = total_fuel_consumed_liters; 
    doc["fuel_consumption_per_hour"] = fuel_consumption_per_hour; 


    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
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
