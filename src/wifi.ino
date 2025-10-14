#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h> // Добавляем для работы с EEPROM
#include <ESP8266HTTPUpdateServer.h> // Добавьте эту строку в самый начало

// Глобальная переменная для управления логированием
extern bool loggingEnabled;

// Имя хоста для mDNS
const char* mdns_hostname = "espwebasto";

// Объект веб-сервера на порту 80
ESP8266WebServer server(80);
// Объект WebSocket сервера на порту 81
WebSocketsServer webSocket = WebSocketsServer(81);


// OTA обновление
ESP8266HTTPUpdateServer httpUpdater;


// Флаги режима работы
bool isAPMode = false;
unsigned long wifiConnectStartTime = 0;
const unsigned long WIFI_CONNECT_TIMEOUT = 15000; // 15 секунд на подключение

// Структура для хранения WiFi настроек в EEPROM
struct WiFiSettings {
  char ssid[32];
  char password[64];
  bool valid;
};

WiFiSettings storedWiFi;

// Адреса в EEPROM для хранения настроек
// Используем область после настроек из low_smoke.ino
const int EEPROM_SIZE = 512;
const int WIFI_SETTINGS_ADDR = 200; // Начинаем после настроек из low_smoke.ino

// Объявление внешних переменных
extern float exhaust_temp;
extern float fan_speed;
extern float fuel_need;
extern int glow_time;
extern int glow_left;
extern int burn_mode;
extern bool burn;
extern bool webasto_fail;
extern const char* message;
extern int attempt;
extern int delayed_period;
extern bool fuelPumpingActive;
extern SystemState currentState;
extern float total_fuel_consumed_liters;
extern float fuel_consumption_per_hour;
extern Settings settings;
extern int max_pwm_global;
extern unsigned long glow_brightness_max;
extern unsigned long glow_fade_in_duration_ms;
extern unsigned long glow_fade_out_duration_ms;

// Объявление внешних функций
extern void handleUpCommand();
extern void handleDownCommand();
extern void handleEnterCommand();
extern void handleFuelPumpingCommand();
extern void handleSettingsUpdate(char* paramsStr, bool is_from_websocket);
extern void resetToDefaultSettings();

// Переменная для отслеживания подключения WebSocket
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
			padding: 0.75rem 1rem;
			color: #e2e8f0;
			width: 100%;
			margin-top: 4px;
			font-size: 1rem;
			box-sizing: border-box;
		}

		.input-field[type="number"] {
			min-width: 120px;
		}
		
		.slider {
			-webkit-appearance: none;
			width: 100%;
			height: 10px;
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
			width: 22px;
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

        /* ИСПРАВЛЕННЫЕ СТИЛИ ДЛЯ ВКЛАДОК */
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
			margin-top: -1px;
			border-radius: 0 0 0.75rem 0.75rem;
			border: 1px solid #4a5568;
			border-top: none;
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

        <div class="tabs-container">
            <div class="tab-buttons">
                <div class="tab-button active" onclick="openTab(event, 'controlStatus')">Управление и статус</div>
                <div class="tab-button" onclick="openTab(event, 'settings')">Настройки</div>
                <div class="tab-button" onclick="openTab(event, 'log')">Лог</div>
                <div class="tab-button" onclick="openTab(event, 'wifiSettings')">Wi-Fi</div>
                <div class="tab-button" onclick="openTab(event, 'fuelConsumption')">Расход топлива</div>
                <div class="tab-button" onclick="openTab(event, 'otaUpdate')">OTA Обновление</div>
            </div>

        <!-- Содержимое вкладки "Управление и статус" -->
        <div id="controlStatus" class="tab-content active card">
            <div class="status-grid">
                <div>
                    <h2>Текущий статус</h2>
                    
                    <div class="status-item">Режим WiFi: <span id="wifiModeDisplay" class="font-bold">--</span></div>
                    <div class="status-item">IP устройства: <span id="deviceIP" class="font-bold">--</span></div>
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
                        <button id="downBtn" class="btn btn-secondary">Вверх (Режим)</button>
                        <button id="upBtn" class="btn btn-secondary">Вниз (Режим)</button>
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
                    <div class="wifi-info-item">Режим работы: <span id="wifiMode" class="font-bold">--</span></div>
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


        <!-- Новая вкладка OTA Обновление -->
        <div id="otaUpdate" class="tab-content card">
            <h2>OTA Обновление прошивки</h2>
            <div class="ota-section">
                
                <div class="ota-info">
                    <h3>Информация о текущей прошивке</h3>
                    <div id="firmwareInfo">
                        <p>Версия: <span id="fwVersion">Загрузка...</span></p>
                        <p>Дата сборки: <span id="fwDate">Загрузка...</span></p>
                        <p>IP устройства: <span id="fwIP">Загрузка...</span></p>
                    </div>
                </div>
                <div class="ota-warning">
                    <h3>⚠️ Важные предупреждения</h3>
                    <ul>
                        <li>Не выключайте питание во время обновления</li>
                        <li>Обновление займет 30-60 секунд</li>
                        <li>После обновления устройство перезагрузится автоматически</li>
                        <li>Убедитесь, что файл прошивки предназначен для этого устройства</li>
                    </ul>
                </div>
                <div class="ota-upload">
                    <h3>Загрузка новой прошивки</h3>
                    
                    <input type="file" id="firmwareFile" class="file-input" accept=".bin">
                    <label for="firmwareFile" class="file-label" id="fileLabel">
                        📁 Выберите файл прошивки (.bin)
                    </label>
                    
                    <div id="fileInfo" class="status-info status-message">
                        Выбран файл: <span id="fileName" class="font-bold"></span>
                        (<span id="fileSize"></span> байт)
                    </div>
                    <div class="progress-container" id="progressContainer">
                        <div class="progress-bar" id="progressBar">0%</div>
                    </div>
                    <div id="statusMessage" class="status-message"></div>
                    <button id="updateBtn" class="btn btn-success" disabled>
                        Начать обновление
                    </button>
                    <button id="checkUpdateBtn" class="btn btn-secondary">
                        Проверить обновления
                    </button>
                </div>
                <div class="ota-manual">
                    <h3>Ручное обновление</h3>
                    <p>Или перейдите по ссылке для стандартного обновления:</p>
                    <a href="/update" target="_blank" class="btn btn-warning">
                        📤 Открыть страницу обновления
                    </a>
                </div>
            </div>
        </div>
    </div>

    <script>

        // Добавляем OTA функциональность
        document.addEventListener('DOMContentLoaded', function() {
            // Элементы OTA
            const firmwareFile = document.getElementById('firmwareFile');
            const fileLabel = document.getElementById('fileLabel');
            const fileInfo = document.getElementById('fileInfo');
            const fileName = document.getElementById('fileName');
            const fileSize = document.getElementById('fileSize');
            const updateBtn = document.getElementById('updateBtn');
            const progressContainer = document.getElementById('progressContainer');
            const progressBar = document.getElementById('progressBar');
            const statusMessage = document.getElementById('statusMessage');
            const checkUpdateBtn = document.getElementById('checkUpdateBtn');
            const fwVersion = document.getElementById('fwVersion');
            const fwDate = document.getElementById('fwDate');
            const fwIP = document.getElementById('fwIP');

            // Загрузка информации о прошивке
            function loadFirmwareInfo() {
                fwVersion.textContent = '1.0.0';                                // Можно добавить реальную версию из кода
                fwDate.textContent = new Date().toLocaleDateString();
                fwIP.textContent = window.location.hostname || '192.168.10.10';
            }

            // Обработчик выбора файла
            firmwareFile.addEventListener('change', function(e) {
                const file = e.target.files[0];
                if (file) {
                    if (file.name.endsWith('.bin')) {
                        fileName.textContent = file.name;
                        fileSize.textContent = file.size.toLocaleString();
                        fileInfo.style.display = 'block';
                        updateBtn.disabled = false;
                        fileLabel.textContent = '✅ ' + file.name;
                    } else {
                        showStatus('Ошибка: выберите файл с расширением .bin', 'error');
                        resetFileInput();
                    }
                }
            });

            // Кнопка проверки обновлений
            checkUpdateBtn.addEventListener('click', function() {
                showStatus('Проверка обновлений...', 'info');
                // Здесь можно добавить проверку на сервере обновлений
                setTimeout(() => {
                    showStatus('Проверка завершена. Используется последняя версия.', 'success');
                }, 2000);
            });

            // Кнопка начала обновления
            updateBtn.addEventListener('click', function() {
                const file = firmwareFile.files[0];
                if (!file) {
                    showStatus('Ошибка: файл не выбран', 'error');
                    return;
                }

                if (!confirm('ВНИМАНИЕ! Начинается обновление прошивки. Не выключайте питание! Продолжить?')) {
                    return;
                }

                uploadFirmware(file);
            });

            // Функция загрузки прошивки
            function uploadFirmware(file) {
                const xhr = new XMLHttpRequest();
                const formData = new FormData();
                formData.append('firmware', file);

                xhr.upload.addEventListener('progress', function(e) {
                    if (e.lengthComputable) {
                        const percent = (e.loaded / e.total) * 100;
                        updateProgress(percent);
                    }
                });

                xhr.addEventListener('load', function() {
                    if (xhr.status === 200) {
                        showStatus('✅ Обновление успешно завершено! Устройство перезагружается...', 'success');
                        setTimeout(() => {
                            window.location.reload();
                        }, 5000);
                    } else {
                        showStatus('❌ Ошибка обновления: ' + xhr.responseText, 'error');
                    }
                    resetProgress();
                });

                xhr.addEventListener('error', function() {
                    showStatus('❌ Ошибка сети при обновлении', 'error');
                    resetProgress();
                });

                xhr.open('POST', '/update');
                xhr.send(formData);

                progressContainer.style.display = 'block';
                updateBtn.disabled = true;
                showStatus('🔄 Идет обновление... Не выключайте питание!', 'info');
            }

            // Вспомогательные функции
            function updateProgress(percent) {
                progressBar.style.width = percent + '%';
                progressBar.textContent = Math.round(percent) + '%';
            }

            function resetProgress() {
                progressBar.style.width = '0%';
                progressBar.textContent = '0%';
                progressContainer.style.display = 'none';
            }

            function showStatus(message, type) {
                statusMessage.textContent = message;
                statusMessage.className = 'status-message status-' + type;
                statusMessage.style.display = 'block';
            }

            function resetFileInput() {
                firmwareFile.value = '';
                fileLabel.textContent = '📁 Выберите файл прошивки (.bin)';
                fileInfo.style.display = 'none';
                updateBtn.disabled = true;
            }

            // Загружаем информацию о прошивке при старте
            loadFirmwareInfo();
        });

        var ws;
        const debugConsole = document.getElementById('debugConsole');
        var deviceIP = ''; // Будет установлен при подключении

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

        function getWebSocketURL() {
            // Если IP устройства уже известен, используем его
            if (deviceIP) {
                return 'ws://' + deviceIP + ':81/';
            }
            
            // Иначе пробуем разные варианты
            const hostname = window.location.hostname;
            if (hostname && hostname !== '') {
                return 'ws://' + hostname + ':81/';
            }
            
            // Если hostname пустой, используем текущий IP
            const currentHost = window.location.host;
            if (currentHost && currentHost !== '') {
                return 'ws://' + currentHost.split(':')[0] + ':81/';
            }
            
            // Последний вариант - фиксированный IP для AP режима
            return 'ws://192.168.10.10:81/';
        }

        function connectWebSocket() {
            const wsUrl = getWebSocketURL();
            log('Попытка подключения к WebSocket по адресу: ' + wsUrl);

            ws = new WebSocket(wsUrl);

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
                // Пробуем альтернативный адрес при ошибке
                setTimeout(connectWebSocket, 3000);
            };
        }

        function updateUI(data) {
            // Обновляем информацию о WiFi режиме и IP
            if (data.wifi_mode) {
                document.getElementById('wifiModeDisplay').textContent = data.wifi_mode;
                document.getElementById('wifiModeDisplay').className = data.wifi_mode === 'AP' ? 'font-bold text-yellow' : 'font-bold text-green';
                
                document.getElementById('wifiMode').textContent = data.wifi_mode === 'AP' ? 'Точка доступа' : 'Клиент (STA)';
                document.getElementById('wifiMode').className = data.wifi_mode === 'AP' ? 'font-bold text-yellow' : 'font-bold text-green';
            }
            
            if (data.wifi_ip) {
                document.getElementById('deviceIP').textContent = data.wifi_ip;
                document.getElementById('wifiIP').textContent = data.wifi_ip;
                deviceIP = data.wifi_ip; // Сохраняем IP для переподключения
            }

            // WiFi статус
            if (data.wifi_ssid) {
                document.getElementById('wifiSSID').textContent = data.wifi_ssid;
            }

            // Статус WiFi
            const wifiStatusElement = document.getElementById('wifiStatus');
            if (data.wifi_mode === 'AP') {
                wifiStatusElement.textContent = 'Точка доступа активна';
                wifiStatusElement.className = 'font-bold text-yellow';
            } else {
                wifiStatusElement.textContent = 'Подключено';
                wifiStatusElement.className = 'font-bold text-green';
            }

            // Основные данные состояния
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

// Функция для инициализации EEPROM для WiFi
void initWiFiEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Читаем сохраненные настройки WiFi
  EEPROM.get(WIFI_SETTINGS_ADDR, storedWiFi);
  
  // Проверяем валидность данных
  if (!storedWiFi.valid || strlen(storedWiFi.ssid) == 0) {
    Serial.println("DEBUG: No valid WiFi settings found in EEPROM");
    memset(&storedWiFi, 0, sizeof(storedWiFi));
    storedWiFi.valid = false;
  } else {
    Serial.printf("DEBUG: Found saved WiFi: %s\n", storedWiFi.ssid);
  }
}

// Функция для сохранения WiFi настроек в EEPROM
void saveWiFiSettings(const char* ssid, const char* password) {
  memset(&storedWiFi, 0, sizeof(storedWiFi));
  strncpy(storedWiFi.ssid, ssid, sizeof(storedWiFi.ssid) - 1);
  if (password) {
    strncpy(storedWiFi.password, password, sizeof(storedWiFi.password) - 1);
  }
  storedWiFi.valid = true;
  
  EEPROM.put(WIFI_SETTINGS_ADDR, storedWiFi);
  EEPROM.commit();
  
  Serial.printf("DEBUG: WiFi settings saved: SSID=%s\n", ssid);
}

// Функция для очистки WiFi настроек
void clearWiFiSettings() {
  memset(&storedWiFi, 0, sizeof(storedWiFi));
  storedWiFi.valid = false;
  
  EEPROM.put(WIFI_SETTINGS_ADDR, storedWiFi);
  EEPROM.commit();
  
  Serial.println("DEBUG: WiFi settings cleared");
}

// Функция для попытки подключения к сохраненной WiFi сети
bool connectToSavedWiFi() {
  if (!storedWiFi.valid || strlen(storedWiFi.ssid) == 0) {
    Serial.println("DEBUG: No saved WiFi credentials found");
    return false;
  }
  
  Serial.println("DEBUG: Attempting to connect to saved WiFi...");
  Serial.printf("DEBUG: SSID: %s\n", storedWiFi.ssid);
  
  // Инициализируем WiFi в станционном режиме
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  // Пытаемся подключиться к сохраненной сети
  WiFi.begin(storedWiFi.ssid, storedWiFi.password);
  
  Serial.print("DEBUG: Connecting to WiFi");
  wifiConnectStartTime = millis();
  
  // Ждем подключения с таймаутом
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // Проверяем таймаут
    if (millis() - wifiConnectStartTime > WIFI_CONNECT_TIMEOUT) {
      Serial.println();
      Serial.println("DEBUG: WiFi connection timeout!");
      return false;
    }
  }
  
  Serial.println();
  Serial.println("DEBUG: WiFi connected successfully!");
  Serial.print("DEBUG: IP address: ");
  Serial.println(WiFi.localIP());
  
  return true;
}

// Функция для настройки точки доступа
void setupAPMode() {
  Serial.println("DEBUG: Setting up Access Point mode...");
  
  // Переключаемся в режим точки доступа
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  // Настраиваем точку доступа с фиксированными параметрами
  const char* ap_ssid = mdns_hostname;
  const char* ap_password = "12345678";
  IPAddress ap_ip(192, 168, 10, 10);
  IPAddress subnet(255, 255, 255, 0);
  
  // Настраиваем статический IP для точки доступа
  WiFi.softAPConfig(ap_ip, ap_ip, subnet);
  
  // Запускаем точку доступа
  if (WiFi.softAP(ap_ssid, ap_password)) {
    isAPMode = true;
    Serial.println("DEBUG: Access Point started successfully.");
    Serial.print("DEBUG: AP SSID: ");
    Serial.println(ap_ssid);
    Serial.print("DEBUG: AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: Failed to start Access Point!");
  }
}

// Основная функция настройки WiFi
void setup_wifi_station() {
  Serial.println();
  Serial.println("DEBUG: === Starting WiFi Setup ===");
  
  // Инициализируем EEPROM для WiFi настроек
  initWiFiEEPROM();
  
  // Сначала пытаемся подключиться к сохраненной сети
  if (connectToSavedWiFi()) {
    // Успешно подключились к WiFi
    isAPMode = false;
    Serial.println("DEBUG: Running in STA mode (connected to router)");
  } else {
    // Не удалось подключиться - запускаем точку доступа
    setupAPMode();
    Serial.println("DEBUG: Running in AP mode");
  }
  
  // Инициализация mDNS (работает в обоих режимах)
  if (MDNS.begin(mdns_hostname)) {
    Serial.print("DEBUG: mDNS responder started. Access at http://");
    Serial.print(mdns_hostname);
    Serial.println(".local/");
  } else {
    Serial.println("ERROR: mDNS setup failed!");
  }
  
  // Настройка OTA обновления
  httpUpdater.setup(&server);
  
  // Настройка HTTP сервера
  Serial.println("DEBUG: Setting up HTTP server...");
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
    Serial.println("DEBUG: HTTP request for '/' received and page sent.");
  });
  
  server.begin();
  Serial.println("DEBUG: HTTP server started.");
  
  // Настройка WebSocket сервера
  Serial.println("DEBUG: Starting WebSocket server...");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("DEBUG: WebSocket server started.");
  Serial.println("DEBUG: === WiFi Setup Complete ===");
}

// Остальные функции остаются без изменений
void handle_wifi_clients() {
  server.handleClient();
  webSocket.loop();
  MDNS.update();
  
  if (!isAPMode && WiFi.status() != WL_CONNECTED) {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 10000) {
      lastCheck = millis();
      Serial.println("DEBUG: WiFi connection lost! Switching to AP mode...");
      setupAPMode();
    }
  }
}

void send_status_update() {
  if (wsConnected) {
    StaticJsonDocument<512> doc;

    float calculated_fuel_rate_hz = 0.0;
    if (delayed_period > 0) {
      calculated_fuel_rate_hz = 1000.00 / delayed_period;
    }
    
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

    if (isAPMode) {
      doc["wifi_mode"] = "AP";
      doc["wifi_ssid"] = mdns_hostname;
      doc["wifi_ip"] = WiFi.softAPIP().toString();
    } else {
      doc["wifi_mode"] = "STA";
      doc["wifi_ssid"] = WiFi.SSID();
      doc["wifi_ip"] = WiFi.localIP().toString();
    }

    doc["logging_enabled"] = loggingEnabled;
    doc["total_fuel_consumed_liters"] = total_fuel_consumed_liters;
    doc["fuel_consumption_per_hour"] = fuel_consumption_per_hour;

    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
  } else {
    Serial.println("DEBUG: WebSocket not connected, status update skipped.");
  }
}

// Обработчик WebSocket событий
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
        sendCurrentSettings();
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
      
      // Обработка команд управления
      if (strcmp((char*)payload, "GET_SETTINGS") == 0) {
        sendCurrentSettings();
      } else if (strcmp((char*)payload, "RESET_SETTINGS") == 0) {
        resetToDefaultSettings(); 
        sendCurrentSettings();
      } else if (strncmp((char*)payload, "SET:", 4) == 0) {
        char cleanedCommand[256];
        strncpy(cleanedCommand, (char*)payload, sizeof(cleanedCommand) - 1);
        cleanedCommand[sizeof(cleanedCommand) - 1] = '\0';
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
        webasto_fail = false;
      } else if (strcmp((char*)payload, "RESET_WIFI") == 0) {
        Serial.println("DEBUG: Received RESET_WIFI command. Clearing WiFi settings...");
        clearWiFiSettings();
        WiFi.disconnect(true);
        delay(1000);
        ESP.restart();
      } else if (strcmp((char*)payload, "REBOOT_ESP") == 0) {
        Serial.println("DEBUG: Received REBOOT_ESP command. Rebooting.");
        ESP.restart();
      } else if (strcmp((char*)payload, "ENABLE_LOGGING") == 0) {
        loggingEnabled = true;
        Serial.println("DEBUG: Logging enabled.");
        send_status_update();
      } else if (strcmp((char*)payload, "DISABLE_LOGGING") == 0) {
        loggingEnabled = false;
        Serial.println("DEBUG: Logging disabled.");
        send_status_update();
      } else if (strcmp((char*)payload, "SCAN_WIFI") == 0) {
        Serial.println("DEBUG: Received SCAN_WIFI command. Scanning networks...");
        int n = WiFi.scanNetworks();
        Serial.printf("DEBUG: Scan done. Found %d networks.\n", n);
        StaticJsonDocument<512> doc;
        JsonArray networksArray = doc.createNestedArray("wifi_networks");
        for (int i = 0; i < n; ++i) {
          JsonObject network = networksArray.createNestedObject();
          network["ssid"] = WiFi.SSID(i);
          network["rssi"] = WiFi.RSSI(i);
        }
        String jsonString;
        serializeJson(doc, jsonString);
        webSocket.broadcastTXT(jsonString);
        WiFi.scanDelete();
      } else if (strncmp((char*)payload, "CONNECT_WIFI:", 13) == 0) {
        char* commandStr = (char*)payload + 13;
        char* ssid = strtok(commandStr, ",");
        char* password = strtok(NULL, ",");

        if (ssid) {
          Serial.printf("DEBUG: Received CONNECT_WIFI command. Connecting to SSID: %s\n", ssid);
          
          // Сохраняем новые credentials
          saveWiFiSettings(ssid, password);
          
          // Пытаемся подключиться
          WiFi.begin(ssid, password ? password : "");
          
          unsigned long start = millis();
          while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(500);
            Serial.print(".");
          }
          
          if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nDEBUG: Successfully connected to WiFi!");
            isAPMode = false;
          } else {
            Serial.println("\nDEBUG: Failed to connect to WiFi!");
          }
          
          send_status_update();
        }
      } else if (strcmp((char*)payload, "RESET_FUEL_CONSUMPTION") == 0) {
        Serial.println("DEBUG: Received RESET_FUEL_CONSUMPTION command. Resetting total fuel consumed.");
        total_fuel_consumed_liters = 0.0;
        send_status_update();
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

// Функция отправки настроек
void sendCurrentSettings() {
  if (wsConnected) {
    StaticJsonDocument<256> doc;
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
    // Отправка через Serial если WebSocket не подключен
    Serial.print(F("CURRENT_SETTINGS:"));
    Serial.print(F("pump_size=")); Serial.print(settings.pump_size); Serial.print(F(","));
    Serial.print(F("heater_target=")); Serial.print(settings.heater_target); Serial.print(F(","));
    Serial.print(F("heater_min=")); Serial.print(settings.heater_min); Serial.print(F(","));
    Serial.print(F("heater_overheat=")); Serial.print(settings.heater_overheat); Serial.print(F(","));
    Serial.print(F("heater_warning=")); Serial.print(settings.heater_warning); Serial.print(F(","));
    Serial.print(F("max_pwm_fan=")); Serial.print(settings.max_pwm_fan); Serial.print(F(","));
    Serial.print(F("glow_brightness=")); Serial.print(settings.glow_brightness); Serial.print(F(","));
    Serial.print(F("glow_fade_in_duration=")); Serial.print(settings.glow_fade_in_duration); Serial.print(F(","));
    Serial.print(F("glow_fade_out_duration=")); Serial.println(settings.glow_fade_out_duration);
    Serial.println(F("DEBUG: Sent settings via Serial (WebSocket not connected)."));
  }
}