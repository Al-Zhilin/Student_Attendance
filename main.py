import serial
import time

# Открытие последовательного порта (для Windows используйте 'COMx')
ser = serial.Serial('COM19', 115200)  # Порт и скорость

# Функция для отправки команды и получения ответа
def send_command(command):
    ser.write(command.encode())  # Отправка команды
    time.sleep(0.1)  # Ожидание ответа от устройства
    response = ser.readline()  # Чтение ответа
    return response.decode().strip()

# Пример команды для установки частоты 1 МГц
command = "FREQ:1000000"  # Установить частоту 1 МГц
response = send_command(command)
print("Ответ устройства:", response)

# Закрытие порта
ser.close()