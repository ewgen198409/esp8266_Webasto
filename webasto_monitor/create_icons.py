from PIL import Image
import os

# Открываем PNG файл
img = Image.open('webasto.png')

# Создаем отдельные ICO файлы для каждого размера
sizes = [16, 32, 48, 64, 128, 256]

for size in sizes:
    try:
        # Изменяем размер изображения
        resized_img = img.resize((size, size), Image.Resampling.LANCZOS)

        # Сохраняем как отдельный ICO файл
        ico_filename = f'webasto_{size}x{size}.ico'
        resized_img.save(ico_filename, format='ICO')

        print(f"Создана иконка: {ico_filename}")

    except Exception as e:
        print(f"Ошибка создания иконки {size}x{size}: {e}")

print("Все иконки созданы!")
