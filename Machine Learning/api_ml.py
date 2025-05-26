from flask import Flask, request, jsonify
import csv
import os

app = Flask(__name__)

# Preguntar al usuario por el nombre de la medición
nombre_medicion = input("¿Qué estás midiendo?: ").strip()

# Crear el nombre del archivo basándonos en la respuesta
CSV_FILE = f'espectroscopia_{nombre_medicion}.csv'

# Lista de canales esperados
expected_channels = ['R', 'S', 'T', 'U', 'V', 'W', 'G', 'H', 'I', 'J', 'K', 'L', 'A', 'B', 'C', 'D', 'E', 'F']

# Si el archivo CSV no existe, lo creamos con los encabezados
if not os.path.exists(CSV_FILE):
    with open(CSV_FILE, 'w', newline='') as f:
        # Definimos los encabezados, agregando "timestamp" como la primera columna
        fieldnames = ['timestamp'] + expected_channels
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

@app.route('/api/as7265x-data', methods=['POST'])
def recibir_datos_as7265x():
    data = request.get_json()

    # Chequear que lleguen todos los canales esperados
    if not all(channel in data for channel in expected_channels):
        return jsonify({"error": "Faltan algunos canales"}), 400

    # Obtener la marca de tiempo actual (usamos un formato estándar)
    from datetime import datetime
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # Crear un diccionario con los valores de los canales y la marca de tiempo
    row_data = {"timestamp": timestamp}
    for channel in expected_channels:
        # Asumimos que los canales que no se reciban estarán con un valor de 0 o algún valor por defecto
        row_data[channel] = data.get(channel, 0)

    # Guardar los datos recibidos en el archivo CSV
    with open(CSV_FILE, 'a', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['timestamp'] + expected_channels)
        writer.writerow(row_data)

    return jsonify({"mensaje": "Datos recibidos correctamente"}), 200

if __name__ == '__main__':
    # Corre el servidor Flask en el puerto 5000
    app.run(host='0.0.0.0', port=5000)
