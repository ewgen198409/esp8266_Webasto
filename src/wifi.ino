#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h> // Библиотека для работы с JSON
#include <ESP8266mDNS.h> // Библиотека для mDNS

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
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
    <style>
        body {
            font-family: 'Inter', sans-serif;
            background-color: #1a202c;
            color: #e2e8f0;
            margin: 0;
            padding: 16px;
        }
		.container {
			max-width: 768px;
			margin-left: auto;
			margin-right: auto;
			display: flex;
			flex-direction: column;
			/* УБРАТЬ gap: 24px; */
		}

		/* Создаем отдельный контейнер для вкладок без отступов */
		.tabs-container {
			display: flex;
			flex-direction: column;
		}
        h1 {
            font-size: 2rem;
            font-weight: bold;
            text-align: center;
            color: #60a5fa;
            margin-bottom: 32px;
        }
        h2 {
            font-size: 1.5rem;
            font-weight: 600;
            margin-bottom: 12px;
        }
        h3 {
            font-size: 1.25rem;
            font-weight: 600;
            margin-bottom: 12px;
            margin-top: 24px;
        }
        .card {
            background-color: #2d3748;
            border-radius: 0.75rem;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
            padding: 24px;
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
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3), 0 1px 3px rgba(0, 0, 0, 0.15);
            transform: translateY(0);
            border: none;
            width: 100%;
        }
        .btn:hover {
            box-shadow: 0 6px 10px rgba(0, 0, 0, 0.4), 0 2px 5px rgba(0, 0, 0, 0.2);
        }
        .btn:active {
            box-shadow: 0 1px 2px rgba(0, 0, 0, 0.2), 0 1px 1px rgba(0, 0, 0, 0.1);
            transform: translateY(2px);
        }
        .btn-primary {
            background-color: #4299e1;
            background-image: linear-gradient(to bottom right, #4299e1, #3182ce);
            color: white;
        }
        .btn-primary:hover {
            background-color: #3182ce;
            background-image: linear-gradient(to bottom right, #3182ce, #2c5282);
        }
        .btn-danger {
            background-color: #e53e3e;
            background-image: linear-gradient(to bottom right, #e53e3e, #c53030);
            color: white;
        }
        .btn-danger:hover {
            background-color: #c53030;
            background-image: linear-gradient(to bottom right, #c53030, #9b2c2c);
        }
        .btn-secondary {
            background-color: #4a5568;
            background-image: linear-gradient(to bottom right, #4a5568, #2d3748);
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
			padding: 0.75rem 1rem; /* Увеличил padding */
			color: #e2e8f0;
			width: 100%;
			margin-top: 4px;
			font-size: 1rem; /* Добавил размер шрифта */
			box-sizing: border-box; /* Важно для правильного расчета ширины */
		}

		/* Специфичные стили для number input */
		.input-field[type="number"] {
			min-width: 120px; /* Минимальная ширина для числовых полей */
		}
		/* Улучшаем внешний вид слайдеров */
		.slider {
			-webkit-appearance: none;
			width: 100%;
			height: 10px; /* Увеличил высоту */
			border-radius: 5px;
			background: #4a5568;
			outline: none;
			opacity: 0.9;
			-webkit-transition: .2s;
			transition: opacity .2s;
			margin-top: 8px;
			margin-bottom: 8px;
		}

		.slider:hover {
			opacity: 1;
		}

		.slider::-webkit-slider-thumb {
			-webkit-appearance: none;
			appearance: none;
			width: 22px; /* Увеличил размер бегунка */
			height: 22px;
			border-radius: 50%;
			background: #4299e1;
			cursor: pointer;
			border: 2px solid #ffffff;
		}

		.slider::-moz-range-thumb {
			width: 22px;
			height: 22px;
			border-radius: 50%;
			background: #4299e1;
			cursor: pointer;
			border: 2px solid #ffffff;
		}
        .status-indicator {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            display: inline-block;
            margin-left: 8px;
        }
        .status-on { background-color: #48bb78; }
        .status-off { background-color: #e53e3e; }
        .status-warn { background-color: #ecc94b; }
        .glow-icon {
            width: 24px;
            height: 24px;
            vertical-align: middle;
            margin-left: 8px;
        }
        .glow-icon.on { fill: #FFD700; }
        .glow-icon.off { fill: #6B7280; }

        /* ИСПРАВЛЕННЫЕ СТИЛИ ДЛЯ ВКЛАДОК - убран отступ */
		.tab-buttons {
			display: flex;
			justify-content: flex-start;
			margin-bottom: 0;
			overflow-x: auto;
			-webkit-overflow-scrolling: touch;
			scrollbar-width: none;
			-ms-overflow-style: none;
		}
		.tab-buttons::-webkit-scrollbar {
			display: none;
		}
		.tab-button {
			flex-shrink: 0;
			padding: 0.5rem 0.8rem;
			text-align: center;
			font-weight: 600;
			cursor: pointer;
			transition: all 0.2s ease-in-out;
			color: #a0aec0;
			background-color: #222b37;
			border-top-left-radius: 0.4rem;
			border-top-right-radius: 0.4rem;
			border: 1px solid #3a4352;
			border-bottom: 1px solid #3a4352;
			margin-right: 0.1rem;
			position: relative;
			z-index: 0;
			font-size: 0.85rem;
		}
		.tab-button:hover {
			background-color: #3182ce;
			color: white;
		}
		.tab-button.active {
			background-color: #2d3748;
			color: white;
			border: 1px solid #4a5568;
			border-bottom: 1px solid #2d3748;
			z-index: 1;
		}
		.tab-content.card {
			margin-top: -1px; /* Отрицательный margin для перекрытия границы */
			border-radius: 0 0 0.75rem 0.75rem;
			border: 1px solid #4a5568;
			border-top: none; /* Убираем верхнюю границу */
		}
		.tab-content {
			display: none;
		}
		.tab-content.active {
			display: block;
		}

        /* Простые CSS классы без Tailwind */
        .control-section {
            display: flex;
            flex-direction: column;
            gap: 16px;
        }
        
        .control-section h2 {
            font-size: 1.25rem;
            font-weight: 600;
            margin-bottom: 12px;
            margin-top: 0;
        }
        
        .button-row {
            display: flex;
            gap: 16px;
        }
        
        .button-row .btn {
            flex: 1;
        }
        
        .status-grid {
            display: grid;
            grid-template-columns: 1fr;
            gap: 16px;
        }
        
        @media (min-width: 768px) {
            .status-grid {
                grid-template-columns: 1fr 1fr;
            }
        }
        
        .status-item {
            margin-bottom: 12px;
            font-size: 1.125rem;
        }
        
        .status-label {
            font-weight: bold;
            margin-right: 8px;
        }
        
        .settings-section {
            display: flex;
            flex-direction: column;
            gap: 16px;
        }
        
        .setting-item {
            margin-bottom: 16px;
        }
        
        .setting-label {
            display: block;
            font-size: 0.875rem;
            font-weight: 500;
            color: #a0aec0;
            margin-bottom: 4px;
        }
        
        .setting-value {
            color: #60a5fa;
            font-weight: bold;
            margin-left: 8px;
        }
        
        .buttons-row {
            display: flex;
            gap: 16px;
        }
        
        .log-section {
            display: flex;
            flex-direction: column;
            gap: 16px;
        }
        
        .debug-console {
            background-color: #2d3748;
            padding: 12px;
            border-radius: 0.375rem;
            font-size: 0.875rem;
            height: 24rem;
            overflow-y: scroll;
            word-break: break-word;
        }
        
        .checkbox-item {
            display: flex;
            align-items: center;
            margin-bottom: 16px;
        }
        
        .checkbox-item input {
            margin-right: 8px;
        }
        
        .wifi-section {
            display: flex;
            flex-direction: column;
            gap: 16px;
        }
        
        .wifi-info {
            margin-bottom: 16px;
        }
        
        .wifi-info-item {
            font-size: 1.125rem;
            margin-bottom: 8px;
        }
        
        .fuel-section {
            display: flex;
            flex-direction: column;
            gap: 16px;
        }
        
        .fuel-info-item {
            font-size: 1.125rem;
            margin-bottom: 8px;
        }
        
        .text-red {
            color: #ef4444;
        }
        
        .text-green {
            color: #22c55e;
        }
        
        .text-yellow {
            color: #fcd34d;
        }
        
        .text-blue {
            color: #60a5fa;
        }
        
        .font-bold {
            font-weight: bold;
        }
        
        .mb-4 {
            margin-bottom: 16px;
        }
        
        .mt-6 {
            margin-top: 24px;
        }
        
        .w-full {
            width: 100%;
        }
        
        .list-disc {
            list-style-type: disc;
            padding-left: 20px;
        }
        
        .cursor-pointer {
            cursor: pointer;
        }
        
        .hover-blue:hover {
            color: #93c5fd;
        }
    </style>
</head>
<body>
	<div class="container">
		<h1>Управление Webasto</h1>

		<!-- ОБЕРТКА ДЛЯ ВКЛАДОК БЕЗ ОТСТУПОВ -->
		<div class="tabs-container">
			<!-- Кнопки вкладок -->
			<div class="tab-buttons">
				<div class="tab-button active" onclick="openTab(event, 'controlStatus')">Управление и статус</div>
				<div class="tab-button" onclick="openTab(event, 'settings')">Настройки</div>
				<div class="tab-button" onclick="openTab(event, 'log')">Лог</div>
				<div class="tab-button" onclick="openTab(event, 'wifiSettings')">Wi-Fi</div>
				<div class="tab-button" onclick="openTab(event, 'fuelConsumption')">Расход топлива</div>
			</div>

        <!-- Содержимое вкладки "Управление и статус" -->
        <div id="controlStatus" class="tab-content active card">
            <div class="status-grid">
                <div>
                    <h2>Текущий статус</h2>
                    
                    <div class="status-item">
                        Состояние: <span id="statusMessage" class="font-bold">Ожидание...</span>
                        <span id="burnStatusIndicator" class="status-indicator status-off"></span>
                    </div>
                    <div class="status-item">Температура выхлопа: <span id="exhaustTemp" class="font-bold">--</span> &deg;C</div>
                    <div class="status-item">Скорость вентилятора: <span id="fanSpeed" class="font-bold">--</span> %</div>
                    <div class="status-item">Расход топлива: <span id="fuelRateHz" class="font-bold">--</span> Гц</div>
                    <div class="status-item">
                        Свеча:
                        <svg id="glowPlugIcon" class="glow-icon off" viewBox="0 0 24 24" fill="currentColor" stroke="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                            <path d="M12 2L9 11H3L13 22L15 13H21L11 2Z" />
                        </svg>
                    </div>
                    <div class="status-item">Режим горения: <span id="burnMode" class="font-bold">--</span></div>
                    <div class="status-item">Попытка запуска: <span id="attempt" class="font-bold">--</span></div>
                    <div class="status-item">Ошибки: <span id="webastoFail" class="font-bold">Нет</span></div>
                    <div class="status-item">Текущий режим: <span id="currentState" class="font-bold">--</span></div>
                    <div class="status-item">Прокачка топлива: <span id="fuelPumpingActive" class="font-bold">Нет</span></div>
                </div>
                <div class="control-section">
                    <h2>Управление</h2>
                    <button id="toggleBurnBtn" class="btn btn-primary">Включить / Выключить</button>
                    <div class="button-row">
                        <button id="downBtn" class="btn btn-secondary">Вниз (Режим)</button>
                        <button id="upBtn" class="btn btn-secondary">Вверх (Режим)</button>
                    </div>
                    <button id="fuelPumpBtn" class="btn btn-secondary">Прокачка топлива</button>
                    <button id="clearFailBtn" class="btn btn-danger">Сбросить ошибку</button>
                </div>
            </div>
        </div>

		<!-- Содержимое вкладки "Настройки" -->
		<div id="settings" class="tab-content card">
			<h2>Настройки</h2>
			<div class="settings-section">
				<div class="setting-item">
					<label for="pumpSize" class="setting-label">Размер насоса:</label>
					<input type="number" id="pumpSize" class="input-field" min="10" max="100" placeholder="10-100">
				</div>
				
				<div class="setting-item">
					<label for="heaterTarget" class="setting-label">
						Целевая температура нагревателя (°C): 
						<span id="heaterTargetValue" class="setting-value">150</span>
					</label>
					<input type="range" id="heaterTarget" class="slider" min="150" max="250" step="1" value="150">
				</div>
				
				<div class="setting-item">
					<label for="heaterMin" class="setting-label">
						Минимальная температура нагревателя (°C): 
						<span id="heaterMinValue" class="setting-value">140</span>
					</label>
					<input type="range" id="heaterMin" class="slider" min="140" max="240" step="1" value="140">
				</div>
				
				<div class="setting-item">
					<label for="heaterOverheat" class="setting-label">
						Температура перегрева (°C): 
						<span id="heaterOverheatValue" class="setting-value">200</span>
					</label>
					<input type="range" id="heaterOverheat" class="slider" min="200" max="300" step="1" value="200">
				</div>
				
				<div class="setting-item">
					<label for="heaterWarning" class="setting-label">
						Температура предупреждения (°C): 
						<span id="heaterWarningValue" class="setting-value">180</span>
					</label>
					<input type="range" id="heaterWarning" class="slider" min="180" max="280" step="1" value="180">
				</div>
				
				<div class="setting-item">
					<label for="maxPwmFan" class="setting-label">
						Макс. ШИМ вентилятора (0-255): 
						<span id="maxPwmFanPercent" class="setting-value">0%</span>
					</label>
					<input type="number" id="maxPwmFan" class="input-field" min="0" max="255" placeholder="0-255">
				</div>
				
				<div class="setting-item">
					<label for="glowBrightness" class="setting-label">
						Яркость свечи накаливания (0-255): 
						<span id="glowBrightnessPercent" class="setting-value">0%</span>
					</label>
					<input type="number" id="glowBrightness" class="input-field" min="0" max="255" placeholder="0-255">
				</div>
				
				<div class="setting-item">
					<label for="glowFadeInDuration" class="setting-label">Время розжига свечи (мс):</label>
					<input type="number" id="glowFadeInDuration" class="input-field" min="0" max="60000" placeholder="0-60000">
				</div>
				
				<div class="setting-item">
					<label for="glowFadeOutDuration" class="setting-label">Время затухания свечи (мс):</label>
					<input type="number" id="glowFadeOutDuration" class="input-field" min="0" max="60000" placeholder="0-60000">
				</div>
				
				<div class="buttons-row">
					<button id="saveSettingsBtn" class="btn btn-primary">Сохранить настройки</button>
					<button id="resetSettingsBtn" class="btn btn-danger">Сбросить настройки</button>
				</div>
				<button id="loadSettingsBtn" class="btn btn-secondary w-full">Загрузить настройки</button>
			</div>
		</div>

        <!-- Содержимое вкладки "Лог" -->
        <div id="log" class="tab-content card">
            <h2>Лог устройства</h2>
            <div class="checkbox-item">
                <input type="checkbox" id="enableLoggingCheckbox">
                <label for="enableLoggingCheckbox">Включить лог</label>
            </div>
            <div class="log-section">
                <div id="debugConsole" class="debug-console">
                    Подключение к устройству...
                </div>
            </div>
        </div>

        <!-- Содержимое вкладки "Wi-Fi" -->
        <div id="wifiSettings" class="tab-content card">
            <h2>Настройки Wi-Fi</h2>
            <div class="wifi-section">
                <div class="wifi-info">
                    <div class="wifi-info-item">Статус подключения: <span id="wifiStatus" class="font-bold">--</span></div>
                    <div class="wifi-info-item">SSID: <span id="wifiSSID" class="font-bold">--</span></div>
                    <div class="wifi-info-item">IP Адрес: <span id="wifiIP" class="font-bold">--</span></div>
                </div>
                <div class="buttons-row">
                    <button id="resetWifiBtn" class="btn btn-danger">Сбросить настройки Wi-Fi</button>
                    <button id="rebootEspBtn" class="btn btn-secondary">Перезагрузить ESP</button>
                </div>
            </div>

            <h3 class="mt-6">Поиск и подключение к Wi-Fi</h3>
            <button id="scanWifiBtn" class="btn btn-primary mb-4">Найти Wi-Fi сети</button>
            <div id="wifiScanResults" class="mb-4">
                <!-- Результаты сканирования будут здесь -->
            </div>
            <div class="wifi-section">
                <div class="setting-item">
                    <label for="manualSSID" class="setting-label">Имя сети (SSID):</label>
                    <input type="text" id="manualSSID" class="input-field" placeholder="Введите SSID">
                </div>
                <div class="setting-item">
                    <label for="manualPassword" class="setting-label">Пароль:</label>
                    <input type="password" id="manualPassword" class="input-field" placeholder="Введите пароль">
                </div>
                <button id="connectWifiBtn" class="btn btn-primary">Подключиться</button>
            </div>
        </div>

        <!-- Содержимое вкладки "Расход топлива" -->
        <div id="fuelConsumption" class="tab-content card">
            <h2>Расход топлива</h2>
            <div class="fuel-section">
                <div class="fuel-info-item">Текущее потребление: <span id="currentFuelConsumed" class="font-bold">--</span> л</div>
                <div class="fuel-info-item">Расчетный расход за час: <span id="hourlyFuelConsumption" class="font-bold">--</span> л/ч</div>
                <button id="resetFuelBtn" class="btn btn-danger">Сбросить текущее потребление</button>
            </div>
        </div>
    </div>

    <script>
        // JavaScript код остается без изменений
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
            const wsUrl = 'ws://192.168.10.10:81/';

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
                    webastoFailElement.classList.add('text-red');
                    webastoFailElement.classList.remove('text-green');
                } else {
                    webastoFailElement.textContent = 'Нет';
                    webastoFailElement.classList.add('text-green');
                    webastoFailElement.classList.remove('text-red');
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
                    wifiStatusElement.classList.remove('text-red', 'text-yellow');
                    wifiStatusElement.classList.add('text-green');
                    wifiSSIDElement.textContent = data.wifi_ssid || '--';
                    wifiIPElement.textContent = data.wifi_ip || '--';
                } else {
                    wifiStatusElement.textContent = 'Настройка AP';
                    wifiStatusElement.classList.remove('text-green', 'text-red');
                    wifiStatusElement.classList.add('text-yellow');
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
                    ul.className = 'list-disc';
                    data.wifi_networks.forEach(network => {
                        const li = document.createElement('li');
                        li.className = 'cursor-pointer hover-blue';
                        li.textContent = `${network.ssid} (RSSI: ${network.rssi} dBm)`;
                        li.addEventListener('click', () => {
                            document.getElementById('manualSSID').value = network.ssid;
                            document.getElementById('manualPassword').value = '';
                        });
                        ul.appendChild(li);
                    });
                    resultsDiv.appendChild(ul);
                } else {
                    resultsDiv.innerHTML = '<p class="text-red">Сети не найдены.</p>';
                }
            }

            // Fuel Consumption Display
            if (data.total_fuel_consumed_liters !== undefined) {
                document.getElementById('currentFuelConsumed').textContent = data.total_fuel_consumed_liters.toFixed(2);
            } else {
                document.getElementById('currentFuelConsumed').textContent = '--';
            }
            if (data.fuel_consumption_per_hour !== undefined) {
                document.getElementById('hourlyFuelConsumption').textContent = data.fuel_consumption_per_hour.toFixed(2);
            } else {
                document.getElementById('hourlyFuelConsumption').textContent = '--';
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
                document.getElementById('wifiScanResults').innerHTML = '<p class="text-yellow">Сканирование...</p>';
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
                tabcontent[i].classList.remove('active');
            }
            tablinks = document.getElementsByClassName("tab-button");
            for (i = 0; i < tablinks.length; i++) {
                tablinks[i].classList.remove("active");
            }
            document.getElementById(tabName).classList.add('active');
            evt.currentTarget.classList.add("active");
        }

        window.onload = function() {
            connectWebSocket();
            document.querySelector('.tab-buttons .tab-button:first-child').classList.add('active');
        };
    </script>
</body>
</html>
)rawliteral";

// Функция для инициализации Wi-Fi в режиме точки доступа
void setup_wifi_station() {
  Serial.println();
  Serial.println("DEBUG: === Starting WiFi Access Point setup ===");
  Serial.flush();

  // Устанавливаем режим сна Wi-Fi в WIFI_NONE_SLEEP для повышения стабильности
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Настраиваем точку доступа с фиксированными параметрами
  const char* ap_ssid = mdns_hostname; // Используем mdns_hostname как SSID
  const char* ap_password = "12345678"; // Фиксированный пароль
  IPAddress ap_ip(192, 168, 10, 10); // Фиксированный IP точки доступа
  IPAddress subnet(255, 255, 255, 0); // Маска подсети
  IPAddress dhcp_start(192, 168, 10, 100); // Начало диапазона DHCP
  IPAddress dhcp_end(192, 168, 10, 110); // Конец диапазона DHCP

  // Настраиваем статический IP для точки доступа
  WiFi.softAPConfig(ap_ip, ap_ip, subnet);

  // Запускаем точку доступа
  if (WiFi.softAP(ap_ssid, ap_password)) {
    Serial.println("DEBUG: Access Point started successfully.");
    Serial.print("DEBUG: AP SSID: ");
    Serial.println(ap_ssid);
    Serial.print("DEBUG: AP Password: ");
    Serial.println(ap_password);
    Serial.print("DEBUG: AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.flush();
  } else {
    Serial.println("ERROR: Failed to start Access Point!");
    Serial.flush();
    return;
  }

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
  Serial.println("DEBUG: === WiFi Access Point setup complete ===");
  Serial.flush();
}

// Функция для обработки клиентов Wi-Fi и WebSocket
void handle_wifi_clients() {
  // В режиме точки доступа всегда обрабатываем клиентов
  server.handleClient();
  webSocket.loop(); // ОЧЕНЬ ВАЖНО: Вызывать webSocket.loop() как можно чаще в loop()
  MDNS.update(); // Обязательно вызывайте в loop() для работы mDNS
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
        Serial.println("DEBUG: Received RESET_WIFI command. Rebooting ESP.");
        Serial.println("DEBUG: WiFi settings cleared. Rebooting to apply changes.");
        ESP.restart(); // Перезагружаем ESP
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

    // Добавляем данные о Wi-Fi (для режима точки доступа)
    doc["wifi_status"] = 3; // WL_CONNECTED - всегда подключено в AP режиме
    doc["wifi_ssid"] = mdns_hostname; // SSID точки доступа
    doc["wifi_ip"] = WiFi.softAPIP().toString(); // IP точки доступа

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
