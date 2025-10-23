import tkinter as tk
from tkinter import ttk, font, messagebox
import serial
import serial.tools.list_ports
from threading import Thread, Event
import queue
import re
import os
import time
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import requests # Добавляем импорт для HTTP-запросов
import json # Добавляем импорт для работы с JSON
from urllib.parse import urlparse # Для парсинга URL

# OTA buffer size constant (must match the one in firmware)
OTA_BUFFER_SIZE = 512

class WebastoMonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Webasto Monitor")

        # Отключаем изменение размера главного окна
        self.root.resizable(False, False)

        # Устанавливаем фиксированный размер окна
        self.root.geometry("830x670")

        self.log_window = None
        self.log_text = None
        self.log_paused = False

        # Параметры настроек по умолчанию
        self.settings_params = {
            'pump_size': "22",
            'heater_target': "195",
            'heater_min': "190",
            'heater_overheat': "210",
            'heater_warning': "200",
            # Добавленные настройки из wifi.txt
            'max_pwm_fan': "255", # Максимальный ШИМ вентилятора
            'glow_brightness': "255", # Яркость свечи накаливания
            'glow_fade_in_duration': "5000", # Время розжига свечи (мс)
            'glow_fade_out_duration': "5000" # Время затухания свечи (мс)
        }
        
        # Данные для графиков
        self.exhaust_temp_data = []
        self.fuel_hz_data = []
        self.max_data_points = 100
        
        # Загрузка шрифта DS-DIGI
        self.ds_digi_font = self.load_ds_digi_font()
        
        # Переменные для хранения данных
        self.data_vars = {
            'webasto_fail': tk.StringVar(value="N/A"),
            'ignit_fail': tk.StringVar(value="N/A"),
            'exhaust_temp': tk.StringVar(value="N/A"),
            'fan_speed': tk.StringVar(value="N/A"),
            'fuel_hz': tk.StringVar(value="N/A"),
            'fuel_need': tk.StringVar(value="N/A"),
            'glow_left': tk.StringVar(value="N/A"),
            'cycle_time': tk.StringVar(value="N/A"),
            'message': tk.StringVar(value="N/A"),
            'final_fuel': tk.StringVar(value="N/A"),
            'current_state': tk.StringVar(value="N/A"),
            'fuel_hour': tk.StringVar(value="N/A"),
            'consumed_liters': tk.StringVar(value="N/A"),
            'in_temp': tk.StringVar(value="N/A")
        }

        # Данные для информации об устройстве
        self.device_info_vars = {
            'firmware_version': tk.StringVar(value="N/A"),
            'wifi_mode': tk.StringVar(value="N/A"),
            'wifi_ssid': tk.StringVar(value="N/A"),
            'wifi_ip': tk.StringVar(value="N/A"),
            'wifi_status': tk.StringVar(value="N/A"),
            'fs_total': tk.StringVar(value="N/A"),
            'fs_used': tk.StringVar(value="N/A"),
            'fs_free': tk.StringVar(value="N/A"),
            'mac_address': tk.StringVar(value="N/A")
        }

        # Переменные для обновлений прошивки
        self.update_status_var = tk.StringVar(value="")
        self.latest_version_var = tk.StringVar(value="")
        
        # Флаг для остановки потока чтения
        self.stop_event = Event()
        self.serial_queue = queue.Queue()
        self.ota_response_queue = queue.Queue() # Очередь для ответов OTA

        # Время последнего запроса информации об устройстве
        self.last_device_info_request = 0
        
        self.ota_upload_in_progress = False # Флаг для отслеживания процесса загрузки OTA

        # Инициализация GUI
        self.setup_ui()

    def load_ds_digi_font(self):
        try:
            font_path = os.path.join(os.path.dirname(__file__), "DS-DIGI.ttf")
            if os.path.exists(font_path):
                return font.Font(family="DS-Digital", size=14, file=font_path)
            else:
                return font.Font(family="Courier", size=14, weight="bold")
        except:
            return font.Font(family="Courier", size=14, weight="bold")

    def setup_ui(self):
        style = ttk.Style()
        style.configure("TLabelframe", padding=5)
        style.configure("TLabelframe.Label", font=('Helvetica', 9, 'bold'))
        
      # Фрейм для настроек порта
        settings_frame = ttk.LabelFrame(self.root, text="COM Port Settings")
        settings_frame.grid(row=0, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        # Главный фрейм для верхней панели
        top_panel = ttk.Frame(self.root)
        top_panel.grid(row=0, column=0, padx=10, pady=5, sticky="ew", columnspan=2)

        # 1. Фрейм настроек COM-порта (слева)
        com_frame = ttk.LabelFrame(top_panel, text="COM Port Settings")
        com_frame.pack(side=tk.LEFT, padx=5, pady=0, fill=tk.Y)
        
        # Элементы COM-порта
        ttk.Label(com_frame, text="Port:").grid(row=0, column=0, padx=5, pady=5)
        self.port_combobox = ttk.Combobox(com_frame, values=self.get_available_ports(), width=12)
        self.port_combobox.grid(row=0, column=1, padx=5, pady=5)
        
        ttk.Label(com_frame, text="Baudrate:").grid(row=0, column=2, padx=5, pady=5)
        self.baud_combobox = ttk.Combobox(com_frame, values=[57600, 9600, 19200, 38400, 115200], width=8)
        self.baud_combobox.grid(row=0, column=3, padx=5, pady=5)
        self.baud_combobox.current(0)
        
        # Кнопка Connect в COM-фрейме
        self.connect_button = ttk.Button(com_frame, text="Connect", command=self.toggle_connection, width=13)
        self.connect_button.grid(row=0, column=4, padx=5, pady=5)

 
        # 2. Фрейм функций (справа) с увеличенной шириной
        func_frame = ttk.LabelFrame(top_panel, text="Functions")
        func_frame.pack(side=tk.RIGHT, padx=5, pady=0, fill=tk.Y)

        # Увеличиваем внутренние отступы фрейма
        func_frame.config(padding=(7, 5, 7, 5))  # (left, top, right, bottom)

        # Кнопки функций с увеличенной шириной
        self.clear_fail_button = ttk.Button(
            func_frame,
            text="Clear Fail",  # Добавим пробел для лучшего отображения
            command=self.send_clear_fail,
            state='disabled',
            width=13  # Увеличиваем ширину кнопок
        )
        self.clear_fail_button.pack(side=tk.LEFT, padx=3, pady=2)

        self.fuel_pumping_button = ttk.Button(
            func_frame,
            text="Fuel Pumping",  # Добавим пробел
            command=self.send_fuel_pumping,
            state='disabled',
            width=13  # Увеличиваем ширину кнопок
        )
        self.fuel_pumping_button.pack(side=tk.LEFT, padx=3, pady=2)

        # Add the new Log button (initially disabled)
        self.log_button = ttk.Button(
            func_frame,
            text="Log",
            command=self.open_log_window,
            state='disabled',
            width=13
        )
        self.log_button.pack(side=tk.LEFT, padx=3, pady=2)

        # Add RebootESP button (initially disabled)
        self.reboot_esp_button = ttk.Button(
            func_frame,
            text="Reboot ESP",
            command=self.reboot_esp,
            state='disabled',
            width=13
        )
        self.reboot_esp_button.pack(side=tk.LEFT, padx=3, pady=2)

        # Настройка минимальной ширины фрейма функций
        func_frame.update_idletasks()  # Обновляем геометрию
        func_frame.config(width=300)  # Устанавливаем желаемую ширину

        # Фрейм для отображения данных
        data_frame = ttk.LabelFrame(self.root, text="Webasto Data")
        data_frame.grid(row=1, column=0, padx=10, pady=5, sticky="nsew")

        # Устанавливаем минимальную высоту для выравнивания
        data_frame.configure(height=200)
        
        # Создаем метки и значения параметров
        self.create_digital_display(data_frame, "Webasto Fail:", "webasto_fail", 0)
        self.create_digital_display(data_frame, "Ign Fail #:", "ignit_fail", 1)
        self.create_digital_display(data_frame, "Exhaust Temp:", "exhaust_temp", 2)
        self.create_digital_display(data_frame, "Fan Speed %:", "fan_speed", 3)
        self.create_digital_display(data_frame, "Fuel HZ:", "fuel_hz", 4)
        self.create_digital_display(data_frame, "Fuel Need:", "fuel_need", 5)
        self.create_digital_display(data_frame, "Glow Left:", "glow_left", 6)
        self.create_digital_display(data_frame, "Cycle Time:", "cycle_time", 7)
        self.create_digital_display(data_frame, "Message:", "message", 8)
        self.create_digital_display(data_frame, "Final Fuel:", "final_fuel", 9)
        self.create_digital_display(data_frame, "State:", "current_state", 10)
        self.create_digital_display(data_frame, "Fuel Litre", "consumed_liters", 11)
        self.create_digital_display(data_frame, "L/hour", "fuel_hour", 12)
        self.create_digital_display(data_frame, "InTemp", "in_temp", 13)
        
        # Фрейм для кнопок управления
        buttons_frame = ttk.Frame(data_frame)
        buttons_frame.grid(row=14, column=0, columnspan=2, pady=(10, 5), sticky="ew")

        # Кнопки управления
        self.up_button = ttk.Button(
            buttons_frame,
            text="Down",
            command=self.send_up_command,
            state='disabled'
        )
        self.up_button.pack(side=tk.LEFT, padx=5, pady=(0, 5), expand=True)

        self.enter_button = ttk.Button(
            buttons_frame,
            text="Start/Stop",
            command=self.send_enter_command,
            state='disabled'
        )
        self.enter_button.pack(side=tk.LEFT, padx=5, pady=(0, 5), expand=True)

        self.down_button = ttk.Button(
            buttons_frame,
            text="Up",
            command=self.send_down_command,
            state='disabled'
        )
        self.down_button.pack(side=tk.LEFT, padx=5, pady=(0, 5), expand=True)

        # Кнопка Settings
        self.settings_button = ttk.Button(
            data_frame,
            text="Settings",
            command=self.open_settings_window
        )
        self.settings_button.grid(row=16, column=0, columnspan=2, pady=(0, 7), sticky="ew", padx=5)

        # В разделе кнопок управления, после кнопки Settings добавьте:
        self.wifi_button = ttk.Button(
            data_frame,
            text="WiFi Settings",
            command=self.open_wifi_settings
        )
        self.wifi_button.grid(row=17, column=0, columnspan=2, pady=(0, 7), sticky="ew", padx=5)

        # Правый столбец - объединяем в один фрейм для равной высоты
        right_column_frame = ttk.Frame(self.root)
        right_column_frame.grid(row=1, column=1, padx=10, pady=5, sticky="nsew", rowspan=2)

        # Фрейм для информации об устройстве (верхняя часть правого столбца)
        device_info_frame = ttk.LabelFrame(right_column_frame, text="Device Information")
        device_info_frame.pack(fill=tk.X, expand=False, padx=0, pady=0)
        device_info_frame.configure(height=120, width=400)  # Фиксированная высота и ширина для информации об устройстве

        # Создаем отображение информации об устройстве
        self.create_device_info_display(device_info_frame)

        # Фрейм для графиков (нижняя часть правого столбца)
        graphs_frame = ttk.LabelFrame(right_column_frame, text="Live Graphs")
        graphs_frame.pack(fill=tk.BOTH, expand=False, padx=0, pady=(5, 0))
        graphs_frame.configure(height=300)  # Фиксированная высота для графиков

        # График температуры выхлопа
        self.setup_graph(graphs_frame, "Exhaust Temp", 0)

        # График для Fuel HZ
        self.setup_graph(graphs_frame, "Fuel HZ", 1)

        # Настройка растягивания
        self.root.columnconfigure(0, weight=1)
        self.root.columnconfigure(1, weight=3)
        self.root.rowconfigure(1, weight=0)     # Информация об устройстве - фиксированная высота
        self.root.rowconfigure(2, weight=0)     # Графики - фиксированная высота

    def create_device_info_display(self, parent):
        # Используем grid с двумя колонками для лучшего использования пространства
        parent.columnconfigure(1, weight=1)
        parent.columnconfigure(3, weight=1)

        # Первая строка
        ttk.Label(parent, text="Firmware:", width=12).grid(row=0, column=0, padx=2, pady=1, sticky="w")
        ttk.Label(parent, textvariable=self.device_info_vars['firmware_version'],
                  background="white", relief="sunken").grid(row=0, column=1, padx=2, pady=1, sticky="ew")

        ttk.Label(parent, text="WiFi Mode:", width=12).grid(row=0, column=2, padx=2, pady=1, sticky="w")
        ttk.Label(parent, textvariable=self.device_info_vars['wifi_mode'],
                  background="white", relief="sunken").grid(row=0, column=3, padx=2, pady=1, sticky="ew")

        # Вторая строка
        ttk.Label(parent, text="SSID:", width=12).grid(row=1, column=0, padx=2, pady=1, sticky="w")
        ttk.Label(parent, textvariable=self.device_info_vars['wifi_ssid'],
                  background="white", relief="sunken").grid(row=1, column=1, padx=2, pady=1, sticky="ew")

        ttk.Label(parent, text="IP:", width=12).grid(row=1, column=2, padx=2, pady=1, sticky="w")
        ttk.Label(parent, textvariable=self.device_info_vars['wifi_ip'],
                  background="white", relief="sunken").grid(row=1, column=3, padx=2, pady=1, sticky="ew")

        # Третья строка
        ttk.Label(parent, text="MAC:", width=12).grid(row=2, column=0, padx=2, pady=1, sticky="w")
        ttk.Label(parent, textvariable=self.device_info_vars['mac_address'],
                  background="white", relief="sunken").grid(row=2, column=1, padx=2, pady=1, sticky="ew")

        ttk.Label(parent, text="WiFi Status:", width=12).grid(row=2, column=2, padx=2, pady=1, sticky="w")
        ttk.Label(parent, textvariable=self.device_info_vars['wifi_status'],
                  background="white", relief="sunken").grid(row=2, column=3, padx=2, pady=1, sticky="ew")

        # Четвертая строка
        ttk.Label(parent, text="FS Total:", width=12).grid(row=3, column=0, padx=2, pady=1, sticky="w")
        ttk.Label(parent, textvariable=self.device_info_vars['fs_total'],
                  background="white", relief="sunken").grid(row=3, column=1, padx=2, pady=1, sticky="ew")

        ttk.Label(parent, text="FS Free:", width=12).grid(row=3, column=2, padx=2, pady=1, sticky="w")
        ttk.Label(parent, textvariable=self.device_info_vars['fs_free'],
                  background="white", relief="sunken").grid(row=3, column=3, padx=2, pady=1, sticky="ew")
                  
                  
    def send_clear_fail(self):
        """Отправка команды сброса ошибки Webasto (аналог длинного нажатия 2 сек)"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'CF\n')
                self.log_message("Sent CLEAR_FAIL command")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send CLEAR_FAIL command: {str(e)}")

    def send_fuel_pumping(self):
        """Отправка команды принудительной подкачки топлива (аналог удержания 10 сек)"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'FP\n')
                self.log_message("Sent FUEL_PUMPING command")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send FUEL_PUMPING command: {str(e)}")

                
    def setup_graph(self, parent, title, row):
        frame = ttk.Frame(parent)
        frame.grid(row=row, column=0, sticky="nsew", padx=5, pady=2)  # растягиваем фрейм

        # Увеличиваем размер фигуры для лучшего заполнения пространства
        fig = Figure(figsize=(5, 2.15), dpi=100)  # Уменьшаем высоту до 1.5
        ax = fig.add_subplot(111)
        ax.set_title(title, fontsize=10)  # Уменьшаем размер шрифта заголовка
        ax.set_facecolor('black')
        fig.patch.set_facecolor('black')
        ax.tick_params(colors='white', labelsize=8)  # Уменьшаем размер шрифта меток
        ax.xaxis.label.set_color('white')
        ax.yaxis.label.set_color('white')
        ax.title.set_color('white')
        
        if title == "Exhaust Temp":
            self.exhaust_temp_line, = ax.plot([], [], 'g-', linewidth=1)  # Тоньше линия
            ax.set_ylim(0, 250)
            self.exhaust_temp_canvas = FigureCanvasTkAgg(fig, master=frame)
            self.exhaust_temp_canvas.draw()
            self.exhaust_temp_canvas.get_tk_widget().pack(fill=tk.BOTH)
            self.exhaust_temp_ax = ax
        else:  # График Fuel HZ
            self.fuel_hz_line, = ax.plot([], [], 'y-', linewidth=1)  # Тоньше линия
            ax.set_ylim(0, 10)
            self.fuel_hz_canvas = FigureCanvasTkAgg(fig, master=frame)
            self.fuel_hz_canvas.draw()
            self.fuel_hz_canvas.get_tk_widget().pack(fill=tk.BOTH)
            self.fuel_hz_ax = ax


    def update_graphs(self):
        # Обновляем график температуры выхлопа
        if len(self.exhaust_temp_data) > 0:
            x_data = range(len(self.exhaust_temp_data))
            self.exhaust_temp_line.set_data(x_data, self.exhaust_temp_data)
            self.exhaust_temp_ax.relim()
            self.exhaust_temp_ax.autoscale_view(True, True, True)
            self.exhaust_temp_canvas.draw()
        
        # Обновляем график частоты топливного насоса
        if len(self.fuel_hz_data) > 0:
            x_data = range(len(self.fuel_hz_data))
            self.fuel_hz_line.set_data(x_data, self.fuel_hz_data)
            self.fuel_hz_ax.relim()
            self.fuel_hz_ax.autoscale_view(True, True, True)
            self.fuel_hz_canvas.draw()
        
        # Планируем следующее обновление
        if not self.stop_event.is_set():
            self.root.after(500, self.update_graphs)

    def create_digital_display(self, frame, label_text, var_name, row):
        row_frame = ttk.Frame(frame)
        row_frame.grid(row=row, column=0, sticky="ew", padx=5, pady=2)

        label = ttk.Label(row_frame, text=label_text, width=15, anchor="e")
        label.grid(row=0, column=0, padx=5, sticky="e")

        value_frame = ttk.Frame(row_frame)
        value_frame.grid(row=0, column=1, sticky="ew")

        value_label = tk.Label(
            value_frame,
            textvariable=self.data_vars[var_name],
            font=self.ds_digi_font,
            fg="#00FF00",
            bg="black",
            padx=5,
            anchor="w"
        )
        value_label.pack(fill=tk.X, expand=True)

        row_frame.columnconfigure(1, weight=1)

    def open_settings_window(self):
        self.settings_window = tk.Toplevel(self.root)
        self.settings_window.title("Settings")
        self.settings_window.geometry("400x570")  # Увеличили высоту для нового раздела обновления прошивки

        # Запрещаем изменение размера окна настроек
        self.settings_window.resizable(False, False)
        
        # Фрейм для параметров настроек
        settings_frame = ttk.LabelFrame(self.settings_window, text="Configuration Parameters")
        settings_frame.pack(fill=tk.X, padx=10, pady=10) # Изменили expand на False, чтобы не занимал все доступное пространство
        
        # Создаем элементы управления для каждого параметра с подсказками
        self.settings_entries = {}
        params = [
            ("Pump Size (ml):", "pump_size", "Размер насоса (любое число, по умолчанию 22). Можно корректировать по мере износа. Больше число = меньше производительность."),
            ("Heater Target Temp (C):", "heater_target", "Целевая температура для нагревателя, градусы Цельсия. При установке больше 250, установятся все значения настроек по-умочанию после перезагрузки"),
            ("Heater Min Temp (C):", "heater_min", "Минимальная температура для нагревателя, градусы Цельсия"),
            ("Heater Overheat Temp (C):", "heater_overheat", "Температура перегрева нагревателя, градусы Цельсия"),
            ("Heater Warning Temp (C):", "heater_warning", "Температура предупреждения для нагревателя, градусы Цельсия"),
            ("Max PWM Fan (0-255):", "max_pwm_fan", "Максимальное значение ШИМ для вентилятора (0-255)."), # НОВАЯ НАСТРОЙКА
            ("Glow Brightness (0-255):", "glow_brightness", "Яркость свечи накаливания (0-255)."), # НОВАЯ НАСТРОЙКА
            ("Glow Fade In Duration (ms):", "glow_fade_in_duration", "Время плавного розжига свечи в миллисекундах."), # НОВАЯ НАСТРОЙКА
            ("Glow Fade Out Duration (ms):", "glow_fade_out_duration", "Время плавного затухания свечи в миллисекундах.") # НОВАЯ НАСТРОЙКА
        ]
        
        for i, (label, param, tooltip) in enumerate(params):
            frame = ttk.Frame(settings_frame)
            frame.grid(row=i, column=0, sticky="ew", padx=5, pady=2)
            
            # Создаем Label с возможностью добавления подсказки
            lbl = ttk.Label(frame, text=label, width=25, anchor="e")
            lbl.grid(row=0, column=0, padx=5, sticky="e")
            
            # Создаем Entry с возможностью добавления подсказки
            entry = ttk.Entry(frame)
            entry.insert(0, self.settings_params.get(param, ""))
            entry.grid(row=0, column=1, sticky="ew")
            
            # Добавляем подсказки к Label и Entry
            self.create_tooltip(lbl, tooltip)
            self.create_tooltip(entry, tooltip)
            
            self.settings_entries[param] = entry
            frame.columnconfigure(1, weight=1)
        
        # Фрейм для предупреждения
        warning_frame = ttk.Frame(self.settings_window)
        warning_frame.pack(fill=tk.X, padx=10, pady=(0, 10))

        warning_label = ttk.Label(
            warning_frame,
            text="Остальные настройки лучше править в коде, сильно зависят друг от друга!!! Считывать и применять настройки когда нагреватель OFF",
            foreground="red",
            wraplength=380,
            justify=tk.CENTER
        )
        warning_label.pack(fill=tk.X)

        # Кнопки управления
        buttons_frame = ttk.Frame(self.settings_window)
        buttons_frame.pack(fill=tk.X, padx=10, pady=(0, 10))

        ttk.Button(
            buttons_frame,
            text="Read Settings",
            command=self.read_settings
        ).pack(side=tk.LEFT, padx=5)

        ttk.Button(
            buttons_frame,
            text="Save Settings",
            command=self.save_settings
        ).pack(side=tk.LEFT, padx=5)

        ttk.Button(
            buttons_frame,
            text="Reset Settings",
            command=self.reset_settings
        ).pack(side=tk.LEFT, padx=5)

        # Новый фрейм для обновления прошивки
        firmware_update_frame = ttk.LabelFrame(self.settings_window, text="Firmware Update")
        firmware_update_frame.pack(fill=tk.BOTH, padx=10, pady=10, expand=True)

        # Конфигурация колонок для равномерного распределения
        firmware_update_frame.columnconfigure(1, weight=1)

        # Ряд 0: Current Firmware Version
        ttk.Label(firmware_update_frame, text="Current Firmware Version:").grid(row=0, column=0, padx=5, pady=2, sticky="w")
        ttk.Label(firmware_update_frame, textvariable=self.device_info_vars['firmware_version'],
                  background="white", relief="sunken").grid(row=0, column=1, padx=5, pady=2, sticky="ew")

        # Ряд 1: Latest Version
        ttk.Label(firmware_update_frame, text="Latest Version:").grid(row=1, column=0, padx=5, pady=2, sticky="w")
        ttk.Label(firmware_update_frame, textvariable=self.latest_version_var,
                  background="white", relief="sunken").grid(row=1, column=1, padx=5, pady=2, sticky="ew")

        # Ряд 2: обновление статуса
        ttk.Label(firmware_update_frame, text="Update Status:").grid(row=2, column=0, columnspan=1, padx=5, pady=2, sticky="w")
        ttk.Label(firmware_update_frame, textvariable=self.update_status_var,
                  background="white", relief="sunken").grid(row=2, column=1, columnspan=1, padx=5, pady=2, sticky="ew")

        # Ряд 3: кнопки
        self.check_update_button = ttk.Button(
            firmware_update_frame,
            text="Check",
            command=self.check_for_firmware_update
        )
        self.check_update_button.grid(row=3, column=0, padx=5, pady=5, sticky="ew")

        self.download_update_button = ttk.Button(
            firmware_update_frame,
            text="Download & Update",
            command=self.download_and_update_firmware,
            state='disabled'
        )
        self.download_update_button.grid(row=3, column=1, padx=5, pady=5, sticky="ew")

        # Ряд 4: прогресс
        self.progress_frame = ttk.Frame(firmware_update_frame)
        self.progress_frame.grid(row=4, column=0, columnspan=2, padx=5, pady=5, sticky="ew")
        self.update_progress = ttk.Progressbar(self.progress_frame, orient="horizontal", mode="determinate")
        self.update_progress.pack(fill=tk.X, expand=True, pady=(0, 2))
        # Заменяем Label на Text для прокрутки текста с горизонтальным скроллбаром
        self.progress_scroll = tk.Scrollbar(self.progress_frame, orient=tk.HORIZONTAL)
        self.progress_text = tk.Text(self.progress_frame, height=1, bg="lightblue", fg="black", wrap="none", font=("Arial", 10),
                                     xscrollcommand=self.progress_scroll.set)
        self.progress_scroll.config(command=self.progress_text.xview)
        self.progress_scroll.pack(side=tk.BOTTOM, fill=tk.X)
        self.progress_text.pack(fill=tk.X, expand=True)
        self.progress_text.insert(1.0, "0%")  # Начальный текст
        self.progress_text.config(state="disabled")  # Изначально только для чтения

    def set_progress_text(self, text):
        """Устанавливает текст в progress_text с включением редактирования."""
        self.progress_text.config(state="normal")
        self.progress_text.delete(1.0, tk.END)
        self.progress_text.insert(1.0, text)
        self.progress_text.config(state="disabled")

    def create_tooltip(self, widget, text):
        """Создает всплывающую подсказку для виджета"""
        tooltip = tk.Toplevel(self.root)
        tooltip.withdraw()
        tooltip.overrideredirect(True)
        
        label = ttk.Label(tooltip, text=text, background="#ffffe0", relief="solid", borderwidth=1, 
                         padding=(5, 3, 5, 3), wraplength=300)
        label.pack()
        
        def enter(event):
            x = widget.winfo_rootx() + widget.winfo_width() + 5
            y = widget.winfo_rooty() + (widget.winfo_height() // 2)
            tooltip.geometry(f"+{x}+{y}")
            tooltip.deiconify()
        
        def leave(event):
            tooltip.withdraw()
        
        widget.bind("<Enter>", enter)
        widget.bind("<Leave>", leave)
        tooltip.bind("<Leave>", leave)

    def read_settings(self):
        if hasattr(self, 'ser') and self.ser.is_open:
            self.ser.write(b'GET_SETTINGS\n')
            self.log_message("Sent GET_SETINGS command")
        else:
            messagebox.showerror("Error", "Not connected to device!")

    def save_settings(self):
        if not hasattr(self, 'ser') or not self.ser.is_open:
            messagebox.showerror("Error", "Not connected to device!")
            return

        settings_command = "SET:"
        settings_command += ",".join(
            f"{k}={v.get()}" for k, v in self.settings_entries.items()
        )
        settings_command += "\n"

        try:
            self.ser.write(settings_command.encode())
            self.log_message(f"Sent settings: {settings_command.strip()}")
            messagebox.showinfo("Success", "Settings sent to device")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to send settings: {str(e)}")

    def reset_settings(self):
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'RESET_SETTINGS\n')
                self.log_message("Sent RESET_SETTINGS command")
                messagebox.showinfo("Success", "Reset settings command sent")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send reset settings: {str(e)}")
        else:
            messagebox.showerror("Error", "Not connected to device!")

    def send_up_command(self):
        """Отправка команды увеличения уровня мощности"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'UP\n')
                self.log_message("Sent UP command")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send UP command: {str(e)}")

    def send_down_command(self):
        """Отправка команды уменьшения уровня мощности"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'DOWN\n')
                self.log_message("Sent DOWN command")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send DOWN command: {str(e)}")

    def send_enter_command(self):
        """Отправка команды включения/выключения нагрева"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'ENTER\n')
                self.log_message("Sent ENTER command")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send ENTER command: {str(e)}")

    def process_serial_queue(self):
        try:
            while not self.serial_queue.empty():
                line = self.serial_queue.get_nowait()

                if self.ota_upload_in_progress or (not self.ota_upload_in_progress and line.startswith("OTA_")):
                    # Если загрузка OTA в процессе, или ранее была OTA, все OTA строки идут в очередь
                    self.ota_response_queue.put(line)
                    # Не обрабатываем другие команды для OTA сообщений
                    continue

                # Существующая логика парсинга для не-OTA команд
                if line.startswith("WIFI_STATUS:"):
                    self.parse_wifi_status(line[12:])
                elif line.startswith("WIFI_SCAN_START"):
                    self.current_scan_results = []
                elif line.startswith("WIFI_SCAN_END"):
                    self.update_wifi_networks_list()
                elif line.startswith("SSID:") and "RSSI:" in line:
                    self.parse_wifi_network(line)
                elif line.startswith("CURRENT_SETTINGS:"):
                    self.parse_settings(line[17:])
                elif line.startswith("SETTINGS_OK"):
                    self.log_message("Device confirmed settings update")
                elif line.startswith("SETTINGS_ERROR"):
                    self.log_message("Device reported settings error")
                # Обработка новой информации об устройстве
                elif line.startswith("FIRMWARE_VERSION:"):
                    self.device_info_vars['firmware_version'].set(line[17:].strip())
                elif line.startswith("WIFI_STATUS_DETAILED:"):
                    # Исправленный срез - убираем "WIFI_STATUS_DETAILED:" (21 символ)
                    self.parse_detailed_wifi_status(line[21:].strip())
                elif line.startswith("FS_TOTAL:"):
                    self.parse_fs_info(line)
                else:
                    # Парсим обычные данные Webasto
                    self.parse_data(line)

        except queue.Empty:
            pass

        # Проверяем, нужно ли повторно запросить информацию об устройстве
        current_time = time.time()
        if current_time - self.last_device_info_request > 5:  # каждые 5 секунд
            any_na = any(var.get() == "N/A" for var in self.device_info_vars.values())
            if any_na and not self.ota_upload_in_progress: # Не запрашиваем во время OTA
                self.request_device_info()

        if not self.stop_event.is_set():
            self.root.after(100, self.process_serial_queue)


    # Добавьте методы для парсинга подробной информации:
    def parse_detailed_wifi_status(self, data):
        """Парсинг подробной информации о WiFi статусе"""
        try:
            self.log_message(f"Parsing WiFi status data: '{data}'")  # Для отладки
            
            params = data.split(',')
            status_info = {}
            for param in params:
                if '=' in param:
                    key, value = param.split('=', 1)  # Разделяем только по первому '='
                    status_info[key.strip()] = value.strip()
            
            self.log_message(f"Parsed WiFi info: {status_info}")  # Для отладки
            
            # Обновляем переменные отображения
            if 'mode' in status_info:
                mode = status_info['mode']
                self.device_info_vars['wifi_mode'].set(mode)
                
                # Определяем SSID и IP в зависимости от режима
                if mode == 'AP':
                    self.device_info_vars['wifi_ssid'].set(status_info.get('ap_ssid', 'N/A'))
                    self.device_info_vars['wifi_ip'].set(status_info.get('ap_ip', 'N/A'))
                    self.device_info_vars['wifi_status'].set('Access Point')
                else:  # STA mode
                    sta_ssid = status_info.get('sta_ssid', 'N/A')
                    sta_status = status_info.get('sta_status', 'N/A')
                    self.device_info_vars['wifi_ssid'].set(sta_ssid)
                    self.device_info_vars['wifi_ip'].set(status_info.get('sta_ip', 'N/A'))
                    if sta_status == 'connected':
                        self.device_info_vars['wifi_status'].set('Connected')
                    else:
                        self.device_info_vars['wifi_status'].set('Disconnected')
            
            if 'mac_address' in status_info:
                self.device_info_vars['mac_address'].set(status_info['mac_address'])
                
            self.log_message("WiFi status parsed and displayed successfully")
                
        except Exception as e:
            self.log_message(f"Error parsing detailed WiFi status: {str(e)}")

    def parse_fs_info(self, data):
        """Парсинг информации о файловой системе"""
        try:
            # Формат: FS_TOTAL:1234567,FS_USED:12345,FS_FREE:1222222
            params = data.split(',')
            fs_info = {}
            for param in params:
                if ':' in param:
                    key, value = param.split(':')
                    fs_info[key.strip()] = value.strip()
            
            # Форматируем размеры в читаемый вид
            if 'FS_TOTAL' in fs_info:
                total = self.format_bytes(int(fs_info['FS_TOTAL']))
                self.device_info_vars['fs_total'].set(total)
            
            if 'FS_USED' in fs_info:
                used = self.format_bytes(int(fs_info['FS_USED']))
                self.device_info_vars['fs_used'].set(used)
            
            if 'FS_FREE' in fs_info:
                free = self.format_bytes(int(fs_info['FS_FREE']))
                self.device_info_vars['fs_free'].set(free)
                
        except Exception as e:
            self.log_message(f"Error parsing FS info: {str(e)}")

    def format_bytes(self, size_bytes):
        """Форматирует размер в байтах в читаемый вид"""
        if size_bytes >= 1024 * 1024:
            return f"{size_bytes / (1024 * 1024):.1f} MB"
        elif size_bytes >= 1024:
            return f"{size_bytes / 1024:.1f} KB"
        else:
            return f"{size_bytes} B"
            
    def parse_wifi_network(self, line):
        """Парсинг информации о WiFi сети из результатов сканирования"""
        try:
            # Формат: "SSID: MyNetwork, RSSI: -45"
            ssid_match = re.search(r'SSID:\s*(.+?)(?=,|$)', line)
            rssi_match = re.search(r'RSSI:\s*(-?\d+)', line)
            
            if ssid_match and rssi_match:
                ssid = ssid_match.group(1).strip()
                rssi = int(rssi_match.group(1))
                
                if hasattr(self, 'current_scan_results'):
                    self.current_scan_results.append((ssid, rssi))
                    
        except Exception as e:
            self.log_message(f"Error parsing WiFi network: {str(e)}")

    def update_wifi_networks_list(self):
        """Обновление списка WiFi сетей в интерфейсе"""
        if hasattr(self, 'current_scan_results') and hasattr(self, 'networks_tree'):
            # Очищаем текущий список
            for item in self.networks_tree.get_children():
                self.networks_tree.delete(item)
            
            # Сортируем по силе сигнала (RSSI)
            sorted_networks = sorted(self.current_scan_results, key=lambda x: x[1], reverse=True)
            
            # Добавляем сети в список
            for ssid, rssi in sorted_networks:
                self.networks_tree.insert("", "end", values=(ssid, f"{rssi} dBm"))
            
            self.log_message(f"Found {len(sorted_networks)} WiFi networks")

    def parse_wifi_status(self, data):
        """Парсинг статуса WiFi из строки WIFI_STATUS"""
        try:
            # Формат: mode=AP,ssid=espwebasto,ip=192.168.10.10,status=Connected
            params = data.split(',')
            status_info = {}
            for param in params:
                if '=' in param:
                    key, value = param.split('=')
                    status_info[key.strip()] = value.strip()
            
            # Обновляем отображение статуса в окне WiFi, если оно открыто
            if hasattr(self, 'wifi_status_vars'):
                if 'mode' in status_info:
                    self.wifi_status_vars['mode'].set(status_info['mode'])
                if 'ssid' in status_info:
                    self.wifi_status_vars['ssid'].set(status_info['ssid'])
                if 'ip' in status_info:
                    self.wifi_status_vars['ip'].set(status_info['ip'])
                if 'status' in status_info:
                    self.wifi_status_vars['status'].set(status_info['status'])
                    
            
        except Exception as e:
            self.log_message(f"Error parsing WiFi status: {str(e)}")

    def get_wifi_status(self):
        """Запрос текущего статуса WiFi"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'GET_WIFI_STATUS\n')
                self.log_message("Sent GET_WIFI_STATUS command")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to get WiFi status: {str(e)}")

    def parse_data(self, line):
        patterns = {
            'webasto_fail': r'F: (\S+)',
            'ignit_fail': r'IgnF#: (\S+)',
            'exhaust_temp': r'ETmp: (\S+)',
            'fan_speed': r'Fan%: (\S+)',
            'fuel_hz': r'FHZ (\S+)',  # Это значение теперь будет использоваться для графика
            'fuel_need': r'FN: (\S+)',
            'glow_left': r'Gl: (\S+)',
            'cycle_time': r'CyTim: (\S+)',
            'message': r'I: (\S+)',
            'final_fuel': r'FinalFuel: (\S+)',
            'consumed_liters': r'TFC: (\S+)',
            'fuel_hour': r'FCH: (\S+)',
            'in_temp': r'InTemp: (\S+)',
            'current_state': r'St: (\S+)'
        }
        
        for var_name, pattern in patterns.items():
            match = re.search(pattern, line)
            if match:
                value = match.group(1)
                
                # Преобразование состояния (0 → FULL, 1 → MID, 2 → LOW)
                if var_name == 'current_state':
                    state_mapping = {'0': 'FULL', '1': 'MID', '2': 'LOW'}
                    value = state_mapping.get(value, value)
                
                self.data_vars[var_name].set(value)

                try:
                    if var_name == "exhaust_temp":
                        temp_str = value[:4]  # Берем первые 4 символа
                        temp = float(temp_str)
                        self.exhaust_temp_data.append(temp)
                        if len(self.exhaust_temp_data) > self.max_data_points:
                            self.exhaust_temp_data.pop(0)
                    
                    elif var_name == "fuel_hz":  # Теперь обновляем график Fuel HZ
                        hz = float(value)
                        self.fuel_hz_data.append(hz)  # Используем данные из "Fuel HZ:"
                        if len(self.fuel_hz_data) > self.max_data_points:
                            self.fuel_hz_data.pop(0)
                            
                except ValueError:
                    pass

    def parse_settings(self, data):
        try:
            params = data.split(',')
            for param in params:
                if '=' in param:
                    key, value = param.split('=')
                    if key in self.settings_entries: # Проверяем, существует ли Entry для этого ключа
                        self.settings_entries[key].delete(0, tk.END)
                        self.settings_entries[key].insert(0, value)
                        self.settings_params[key] = value # Обновляем внутренние параметры
            self.log_message("Updated settings from device")
        except Exception as e:
            self.log_message(f"Error parsing settings: {str(e)}")

    def get_available_ports(self):
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports]

    def toggle_connection(self):
        if hasattr(self, 'ser') and self.ser.is_open:
            self.disconnect_serial()
            self.connect_button.config(text="Connect")
            # Отключаем все кнопки управления при разрыве соединения
            self.up_button.config(state='disabled')
            self.down_button.config(state='disabled')
            self.enter_button.config(state='disabled')
            self.clear_fail_button.config(state='disabled')
            self.fuel_pumping_button.config(state='disabled')
            self.log_button.config(state='disabled')
            self.reboot_esp_button.config(state='disabled')
        else:
            if self.connect_serial():
                self.connect_button.config(text="Disconnect")
                self.root.after(500, self.update_graphs)
                # Включаем кнопки управления при успешном подключении
                self.up_button.config(state='normal')
                self.down_button.config(state='normal')
                self.enter_button.config(state='normal')
                self.clear_fail_button.config(state='normal')
                self.fuel_pumping_button.config(state='normal')
                self.log_button.config(state='normal')
                self.reboot_esp_button.config(state='normal')
                
                # Отправляем команды для получения информации об устройстве
                self.root.after(1000, self.request_device_info)

    # Добавьте метод для запроса информации об устройстве:
    def request_device_info(self):
        """Отправляет команды для получения информации об устройстве"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                # Отправляем команды последовательно с задержкой
                self.ser.write(b'GET_FIRMWARE_VERSION\n')
                self.root.after(200, lambda: self.ser.write(b'GET_WIFI_STATUS_DETAILED\n'))
                self.root.after(400, lambda: self.ser.write(b'GET_FS_INFO\n'))
                self.last_device_info_request = time.time()
                self.log_message("Requested device information")
            except Exception as e:
                self.log_message(f"Error requesting device info: {str(e)}")
            
    def connect_serial(self):
        port = self.port_combobox.get()
        baudrate = int(self.baud_combobox.get())
        
        try:
            self.ser = serial.Serial(port, baudrate, timeout=1)
            self.stop_event.clear()
            self.read_thread = Thread(target=self.read_from_port, daemon=True)
            self.read_thread.start()
            self.root.after(100, self.process_serial_queue)
            return True
        except Exception as e:
            messagebox.showerror("Error", f"Connection error: {str(e)}")
            return False

    def disconnect_serial(self):
        self.stop_event.set()
        if hasattr(self, 'ser') and self.ser.is_open:
            self.ser.close()

    def read_from_port(self):
        while not self.stop_event.is_set():
            if hasattr(self, 'ser') and self.ser.is_open:
                try:
                    line = self.ser.readline().decode('utf-8', errors='replace').strip()
                    if line:
                        self.serial_queue.put(line)
                        self.log_message(line)  # Log all incoming data
                except Exception as e:
                    error_msg = f"Read error: {str(e)}"
                    self.serial_queue.put(error_msg)
                    self.log_message(error_msg)
                    break

    def toggle_log_pause(self):
        """Переключение состояния паузы лога"""
        self.log_paused = not self.log_paused
        if self.log_paused:
            self.pause_button.config(text="Resume")
            self.log_message("Log paused")
        else:
            self.pause_button.config(text="Pause")
            self.log_message("Log resumed")

    def log_message(self, message):
        """Enhanced log_message that writes to both console and log window"""
        print(message)  # Console output

        if self.log_text and self.log_window and self.log_window.winfo_exists() and not self.log_paused:
            self.log_text.config(state='normal')
            self.log_text.insert(tk.END, message + "\n")
            self.log_text.see(tk.END)
            self.log_text.config(state='disabled')

    def on_closing(self):
        self.disconnect_serial()
        self.root.destroy()

    def open_log_window(self):
        if self.log_window is None or not self.log_window.winfo_exists():
            self.log_window = tk.Toplevel(self.root)
            self.log_window.title("Serial Port Log")
            self.log_window.geometry(f"{self.root.winfo_width()}x{self.root.winfo_height()}")
            
            # Create main frame
            main_frame = ttk.Frame(self.log_window)
            main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
            
            # Create text widget for log display
            self.log_text = tk.Text(
                main_frame,
                wrap=tk.WORD,
                state='disabled',
                bg='black',
                fg='white',
                font=('Courier', 10)
            )
            self.log_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
            
            # Create scrollbar
            scrollbar = ttk.Scrollbar(main_frame, command=self.log_text.yview)
            scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
            self.log_text.config(yscrollcommand=scrollbar.set)
            
            # Create bottom frame for entry and buttons
            bottom_frame = ttk.Frame(main_frame)
            bottom_frame.pack(fill=tk.X, padx=5, pady=5)
            
            # Entry for sending commands
            self.log_entry = ttk.Entry(bottom_frame)
            self.log_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
            self.log_entry.bind('<Return>', self.send_serial_command)
            
            # Send button
            send_button = ttk.Button(
                bottom_frame,
                text="Send",
                command=self.send_serial_command
            )
            send_button.pack(side=tk.LEFT, padx=5)
            
            # Clear button
            clear_button = ttk.Button(
                bottom_frame,
                text="Clear",
                command=self.clear_log
            )
            clear_button.pack(side=tk.LEFT, padx=5)

            # Pause/Resume button
            self.pause_button = ttk.Button(
                bottom_frame,
                text="Pause",
                command=self.toggle_log_pause
            )
            self.pause_button.pack(side=tk.LEFT, padx=5)
            
            # Configure window close behavior
            self.log_window.protocol("WM_DELETE_WINDOW", self.close_log_window)
        else:
            self.log_window.lift()

    def close_log_window(self):
        if self.log_window:
            self.log_window.destroy()
            self.log_window = None

    def clear_log(self):
        if self.log_text:
            self.log_text.config(state='normal')
            self.log_text.delete(1.0, tk.END)
            self.log_text.config(state='disabled')

    def send_serial_command(self, event=None):
        if hasattr(self, 'ser') and self.ser.is_open and self.log_entry:
            command = self.log_entry.get()
            if command:
                try:
                    self.ser.write((command + '\n').encode())
                    self.log_message(f">>> {command}")
                    self.log_entry.delete(0, tk.END)
                except Exception as e:
                    self.log_message(f"Error sending command: {str(e)}")
    


    def open_wifi_settings(self):
        self.wifi_window = tk.Toplevel(self.root)
        self.wifi_window.title("WiFi Settings")
        self.wifi_window.geometry("300x500")

        # Запрещаем изменение размера окна WiFi настроек
        self.wifi_window.resizable(False, False)
        
        # Фрейм для текущего статуса WiFi
        status_frame = ttk.LabelFrame(self.wifi_window, text="Current WiFi Status")
        status_frame.pack(fill=tk.X, padx=10, pady=5)
        
        # Переменные для статуса
        self.wifi_status_vars = {
            'mode': tk.StringVar(value="N/A"),
            'ssid': tk.StringVar(value="N/A"),
            'ip': tk.StringVar(value="N/A"),
            'status': tk.StringVar(value="N/A")
        }
        
        # Отображение статуса
        ttk.Label(status_frame, text="Mode:").grid(row=0, column=0, sticky="w", padx=5, pady=2)
        ttk.Label(status_frame, textvariable=self.wifi_status_vars['mode']).grid(row=0, column=1, sticky="w", padx=5, pady=2)
        
        ttk.Label(status_frame, text="SSID:").grid(row=1, column=0, sticky="w", padx=5, pady=2)
        ttk.Label(status_frame, textvariable=self.wifi_status_vars['ssid']).grid(row=1, column=1, sticky="w", padx=5, pady=2)
        
        ttk.Label(status_frame, text="IP:").grid(row=2, column=0, sticky="w", padx=5, pady=2)
        ttk.Label(status_frame, textvariable=self.wifi_status_vars['ip']).grid(row=2, column=1, sticky="w", padx=5, pady=2)
        
        ttk.Label(status_frame, text="Status:").grid(row=3, column=0, sticky="w", padx=5, pady=2)
        ttk.Label(status_frame, textvariable=self.wifi_status_vars['status']).grid(row=3, column=1, sticky="w", padx=5, pady=2)
        
        # Фрейм для управления WiFi
        control_frame = ttk.LabelFrame(self.wifi_window, text="WiFi Control")
        control_frame.pack(fill=tk.X, padx=10, pady=5)
        
        # Кнопки управления
        ttk.Button(control_frame, text="Scan Networks", 
                   command=self.scan_wifi_networks).pack(side=tk.LEFT, padx=5, pady=5)
        ttk.Button(control_frame, text="Reset WiFi", 
                   command=self.reset_wifi).pack(side=tk.LEFT, padx=5, pady=5)
        ttk.Button(control_frame, text="Reboot ESP", 
                   command=self.reboot_esp).pack(side=tk.LEFT, padx=5, pady=5)
        
        # Фрейм для подключения к WiFi
        connect_frame = ttk.LabelFrame(self.wifi_window, text="Connect to WiFi")
        connect_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        ttk.Label(connect_frame, text="SSID:").grid(row=0, column=0, sticky="w", padx=5, pady=2)
        self.wifi_ssid_entry = ttk.Entry(connect_frame, width=30)
        self.wifi_ssid_entry.grid(row=0, column=1, padx=5, pady=2, sticky="ew")
        
        ttk.Label(connect_frame, text="Password:").grid(row=1, column=0, sticky="w", padx=5, pady=2)
        self.wifi_password_entry = ttk.Entry(connect_frame, width=30, show="*")
        self.wifi_password_entry.grid(row=1, column=1, padx=5, pady=2, sticky="ew")
        
        ttk.Button(connect_frame, text="Connect", 
                   command=self.connect_to_wifi).grid(row=2, column=0, columnspan=2, pady=10)
        
        # Фрейм для результатов сканирования
        self.scan_frame = ttk.LabelFrame(self.wifi_window, text="Available Networks")
        self.scan_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # Создаем Treeview для отображения сетей
        self.networks_tree = ttk.Treeview(self.scan_frame, columns=("SSID", "RSSI"), show="headings", height=6)
        self.networks_tree.heading("SSID", text="SSID")
        self.networks_tree.heading("RSSI", text="Signal (RSSI)")
        self.networks_tree.column("SSID", width=120)
        self.networks_tree.column("RSSI", width=20)
        
        scrollbar = ttk.Scrollbar(self.scan_frame, orient=tk.VERTICAL, command=self.networks_tree.yview)
        self.networks_tree.configure(yscrollcommand=scrollbar.set)
        
        self.networks_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Привязываем двойной клик к выбору сети
        self.networks_tree.bind("<Double-1>", self.on_network_select)
        
        # Запрашиваем текущий статус WiFi
        self.get_wifi_status()
        
    def get_wifi_status(self):
        """Запрос текущего статуса WiFi"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'GET_WIFI_STATUS\n')
                self.log_message("Sent GET_WIFI_STATUS command")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to get WiFi status: {str(e)}")

    def scan_wifi_networks(self):
        """Сканирование доступных WiFi сетей"""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'SCAN_WIFI\n')
                self.log_message("Sent SCAN_WIFI command")
                # Очищаем список сетей
                for item in self.networks_tree.get_children():
                    self.networks_tree.delete(item)
            except Exception as e:
                messagebox.showerror("Error", f"Failed to scan WiFi networks: {str(e)}")

    def reset_wifi(self):
        """Сброс настроек WiFi"""
        if messagebox.askyesno("Confirm", "Reset WiFi settings? This will restart the device."):
            if hasattr(self, 'ser') and self.ser.is_open:
                try:
                    self.ser.write(b'RESET_WIFI\n')
                    self.log_message("Sent RESET_WIFI command")
                except Exception as e:
                    messagebox.showerror("Error", f"Failed to reset WiFi: {str(e)}")

    def reboot_esp(self):
        """Перезагрузка ESP"""
        if messagebox.askyesno("Confirm", "Reboot ESP device?"):
            if hasattr(self, 'ser') and self.ser.is_open:
                try:
                    self.ser.write(b'REBOOT_ESP\n')
                    self.log_message("Sent REBOOT_ESP command")
                except Exception as e:
                    messagebox.showerror("Error", f"Failed to reboot ESP: {str(e)}")

    def connect_to_wifi(self):
        """Подключение к выбранной WiFi сети"""
        ssid = self.wifi_ssid_entry.get().strip()
        password = self.wifi_password_entry.get().strip()
        
        if not ssid:
            messagebox.showerror("Error", "Please enter SSID")
            return
        
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                command = f"CONNECT_WIFI:{ssid},{password}\n"
                self.ser.write(command.encode())
                self.log_message(f"Sent CONNECT_WIFI command for SSID: {ssid}")
                messagebox.showinfo("Success", "WiFi connection command sent. Device will attempt to connect.")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send WiFi connection command: {str(e)}")

    def on_network_select(self, event):
        """Обработка выбора сети из списка"""
        selection = self.networks_tree.selection()
        if selection:
            item = selection[0]
            ssid = self.networks_tree.item(item, "values")[0]
            self.wifi_ssid_entry.delete(0, tk.END)
            self.wifi_ssid_entry.insert(0, ssid)
            self.wifi_password_entry.focus()
            
    def check_for_firmware_update(self):
        """Проверяет наличие новой версии прошивки на GitHub."""
        self.log_message("Checking for firmware updates...")
        self.check_update_button.config(state='disabled')
        self.download_update_button.config(state='disabled')
        self.update_progress['value'] = 0
        self.set_progress_text("0%")

        # Запускаем проверку в отдельном потоке, чтобы не блокировать GUI
        Thread(target=self._perform_firmware_check, daemon=True).start()

    def _perform_firmware_check(self):
        try:
            response = requests.get("https://api.github.com/repos/ewgen198409/esp8266_Webasto/releases/latest")
            response.raise_for_status()  # Вызывает исключение для плохих статусов HTTP (4xx или 5xx)
            latest_release = response.json()
            
            latest_version = latest_release['tag_name']
            
            # Ищем .bin файл в активах релиза
            download_url = None
            for asset in latest_release['assets']:
                if asset['name'].endswith('.bin'):
                    download_url = asset['browser_download_url']
                    break

            if not download_url:
                self.log_message("No .bin file found in the latest release assets.")
                self.root.after(0, lambda: self.update_status_var.set("No .bin file found"))
                self.root.after(0, lambda: self.latest_version_var.set(latest_version))
                self.root.after(0, lambda: self.check_update_button.config(state='normal'))
                return

            current_version = self.device_info_vars['firmware_version'].get()

            self.log_message(f"Latest version: {latest_version}, Current version: {current_version}")

            if current_version != "N/A" and self._compare_versions(latest_version, current_version) > 0:
                self.latest_firmware_url = download_url
                self.log_message(f"New firmware version available: {latest_version}")
                self.root.after(0, lambda: self.update_status_var.set("New version available!"))
                self.root.after(0, lambda: self.latest_version_var.set(latest_version))
                self.root.after(0, lambda: self.download_update_button.config(state='normal'))
            else:
                self.log_message("No new firmware updates available.")
                self.root.after(0, lambda: self.update_status_var.set("No new updates available"))
                self.root.after(0, lambda: self.latest_version_var.set(latest_version))
                self.root.after(0, lambda: self.set_progress_text("Версия прошивки находится в актуальном состоянии"))

        except requests.exceptions.RequestException as e:
            self.log_message(f"Error checking for firmware updates: {e}")
            self.root.after(0, lambda: self.update_status_var.set("Error checking updates"))
            self.root.after(0, lambda: self.latest_version_var.set("N/A"))
        except json.JSONDecodeError as e:
            self.log_message(f"Error parsing GitHub API response: {e}")
            self.root.after(0, lambda: self.update_status_var.set("Error parsing response"))
            self.root.after(0, lambda: self.latest_version_var.set("N/A"))
        finally:
            self.root.after(0, lambda: self.check_update_button.config(state='normal'))

    def _compare_versions(self, v1, v2):
        """Сравнивает две версии в формате 'vX.Y.Z'."""
        # Удаляем префикс 'v' и разбиваем по точкам
        v1_parts = [int(p) for p in v1.lstrip('v').split('.')]
        v2_parts = [int(p) for p in v2.lstrip('v').split('.')]
        
        # Дополняем более короткую версию нулями, чтобы они были одинаковой длины
        max_len = max(len(v1_parts), len(v2_parts))
        v1_parts += [0] * (max_len - len(v1_parts))
        v2_parts += [0] * (max_len - len(v2_parts))

        for i in range(max_len):
            if v1_parts[i] > v2_parts[i]:
                return 1
            if v1_parts[i] < v2_parts[i]:
                return -1
        return 0 # Версии равны

    def download_and_update_firmware(self):
        """Загружает и устанавливает новую прошивку."""
        if not hasattr(self, 'latest_firmware_url') or not self.latest_firmware_url:
            messagebox.showerror("Error", "No firmware update URL available.")
            return
        
        if not hasattr(self, 'ser') or not self.ser.is_open:
            messagebox.showerror("Error", "Not connected to device! Please connect to COM port first.")
            return

        self.log_message(f"Downloading firmware from: {self.latest_firmware_url}")
        self.download_update_button.config(state='disabled')
        self.check_update_button.config(state='disabled')
        self.update_progress['value'] = 0
        self.update_progress['maximum'] = 100 # Устанавливаем максимум для прогресс-бара

        # Запускаем загрузку и обновление в отдельном потоке
        Thread(target=self._perform_firmware_update, daemon=True).start()

    def _perform_firmware_update(self):
        firmware_path = None
        try:
            # 1. Загрузка файла прошивки
            response = requests.get(self.latest_firmware_url, stream=True)
            response.raise_for_status()

            total_size = int(response.headers.get('content-length', 0))
            block_size = 1024 # 1 KB
            downloaded_size = 0

            firmware_path = os.path.join(os.path.dirname(__file__), "firmware.bin")
            with open(firmware_path, 'wb') as f:
                for data in response.iter_content(block_size):
                    f.write(data)
                    downloaded_size += len(data)
                    progress = (downloaded_size / total_size) * 50 # Первые 50% на загрузку
                    self.root.after(0, lambda p=progress: (
                        self.update_progress.config(value=p),
                        self.set_progress_text(f"{p:.1f}%")
                    )[0])

            self.log_message("Firmware downloaded successfully.")
            self.root.after(0, lambda: (
                self.update_progress.config(value=50),
                self.set_progress_text("50.0%")
            )[0]) # Загрузка завершена

            # 2. Отправка команды START_OTA на устройство
            self.ota_upload_in_progress = True # Устанавливаем флаг для перенаправления ответов перед OTA командами
            self.ser.write(b'START_OTA\n')
            self.log_message("Sent START_OTA command.")

            # Ждем ответа OTA_READY
            response_line = self._wait_for_ota_response("OTA_READY", timeout=10)
            if response_line != "OTA_READY":
                raise Exception(f"Device did not respond with OTA_READY. Got: {response_line}")
            self.log_message("Device is ready for OTA upload.")

            # 3. Отправка файла прошивки по частям
            
            with open(firmware_path, 'rb') as f:
                bytes_sent = 0
                while True:
                    chunk = f.read(OTA_BUFFER_SIZE) # Читаем по 512 байт
                    if not chunk:
                        break

                    self.ser.write(chunk)

                    # Ждем подтверждения приема чанка для синхронизации
                    response = self._wait_for_ota_response("OTA_CHUNK_ACK", timeout=5)
                    if response != "OTA_CHUNK_ACK":
                        raise Exception(f"Device did not acknowledge chunk. Got: {response}")

                    bytes_sent += len(chunk)

                    # Обновляем прогресс-бар (50% - 100% для загрузки на устройство)
                    progress = 50 + (bytes_sent / total_size) * 50
                    self.root.after(0, lambda p=progress: (
                        self.update_progress.config(value=p),
                        self.set_progress_text(f"{p:.1f}%")
                    )[0])

            # 4. Отправка команды END_OTA
            self.ser.write(b'END_OTA\n')
            self.log_message("Sent END_OTA command.")
            
            # Ждем ответа OTA_RECEIVE_COMPLETE
            response_line = self._wait_for_ota_response("OTA_RECEIVE_COMPLETE", timeout=30)
            if not response_line.startswith("OTA_RECEIVE_COMPLETE:"):
                raise Exception(f"Device did not confirm OTA_RECEIVE_COMPLETE. Got: {response_line}")
            self.log_message(f"Device confirmed firmware reception: {response_line}")

            # После получения подтверждения, разрешаем устройству принимать текстовые команды
            self.ota_upload_in_progress = False

            # Ждем ответа OTA_READY_TO_APPLY
            response_line = self._wait_for_ota_response("OTA_READY_TO_APPLY", timeout=10)
            if response_line != "OTA_READY_TO_APPLY":
                raise Exception(f"Device not ready to apply OTA. Got: {response_line}")
            self.log_message("Device is ready to apply firmware.")

            # 5. Отправка команды APPLY_OTA
            self.ser.write(b'APPLY_OTA\n')
            self.log_message("Sent APPLY_OTA command.")

            # Ждем ответа OTA_APPLYING
            response_line = self._wait_for_ota_response("OTA_APPLYING", timeout=10)
            if response_line != "OTA_APPLYING":
                raise Exception(f"Device did not confirm OTA_APPLYING. Got: {response_line}")
            self.log_message("Device is applying firmware.")
            
            # Ждем ответа OTA_REBOOTING
            response_line = self._wait_for_ota_response("OTA_REBOOTING", timeout=10)
            if response_line != "OTA_REBOOTING":
                raise Exception(f"Device did not confirm OTA_REBOOTING. Got: {response_line}")
            self.log_message("Device is rebooting.")

            self.log_message("Firmware update complete! Device will reboot.")
            self.set_progress_text("Процесс обновления завершён. Ожидание перезагрузки устройства...")

            # После обновления, запросим информацию об устройстве снова через большее время
            self.root.after(15000, self._request_firmware_version_after_update) # Даем 15 секунд на перезагрузку и инициализацию
            
        except requests.exceptions.RequestException as e:
            self.log_message(f"Error during firmware download: {e}")
            messagebox.showerror("Error", f"Failed to download firmware: {e}")
        except Exception as e:
            self.log_message(f"Error during firmware update: {e}")
            messagebox.showerror("Error", f"An error occurred during firmware update: {e}")
        finally:
            self.ota_upload_in_progress = False # Снимаем флаг
            self.root.after(0, lambda: self.download_update_button.config(state='normal'))
            self.root.after(0, lambda: self.check_update_button.config(state='normal'))
            self.root.after(0, lambda: (
                self.update_progress.config(value=0),
                self.set_progress_text("0.0%")
            )[0])
            # Удаляем временный файл прошивки
            if firmware_path and os.path.exists(firmware_path):
                os.remove(firmware_path)
                self.log_message(f"Removed temporary firmware file: {firmware_path}")

    def _wait_for_ota_response(self, expected_prefix, timeout=10):
        """Ждет ответа от устройства в течение заданного таймаута."""
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                line = self.ota_response_queue.get(timeout=0.1) # Неблокирующее чтение
                self.log_message(f"OTA Response: {line}")
                if line.startswith(expected_prefix):
                    return line
                elif line.startswith("OTA_ERROR") or line.startswith("OTA_FAIL"):
                    raise Exception(f"Device reported OTA error: {line}")
            except queue.Empty:
                pass
        raise Exception(f"Timeout waiting for OTA response: {expected_prefix}")

    def _request_firmware_version_after_update(self):
        """Запрос только версии прошивки после обновления."""
        if hasattr(self, 'ser') and self.ser.is_open:
            try:
                self.ser.write(b'GET_FIRMWARE_VERSION\n')
                self.log_message("Requested firmware version after update")
            except Exception as e:
                self.log_message(f"Error requesting firmware version: {str(e)}")

    def _is_valid_ip(self, ip_string):
        """Проверяет, является ли строка действительным IP-адресом."""
        pattern = r"^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$"
        match = re.match(pattern, ip_string)
        if not match:
            return False
        for part in match.groups():
            if not (0 <= int(part) <= 255):
                return False
        return True

if __name__ == "__main__":
    root = tk.Tk()
    app = WebastoMonitorApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()
