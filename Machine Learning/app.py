import streamlit as st
import pandas as pd
import os
import matplotlib.pyplot as plt
import joblib
import subprocess
import threading

import csv
from sklearn.ensemble import RandomForestClassifier
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report
from flask import Flask, request, jsonify
from datetime import datetime

st.set_page_config(page_title="Detector de Materiales", layout="centered")
st.title("🔬 Identificador de Materiales con Sensor AS7265x")

# Constantes
CANALES = ['A','B','C','D','E','F','G','H','R','I','S','J','T','U','V','W','K','L']
MODELOS_DIR = "modelos_guardados"
os.makedirs(MODELOS_DIR, exist_ok=True)

# Inicializar session state
if "modelo" not in st.session_state:
    st.session_state.modelo = None
if "escalador" not in st.session_state:
    st.session_state.escalador = None
if "modelo_entrenado" not in st.session_state:
    st.session_state.modelo_entrenado = False

# ------------------ Sección 1: Servidor Flask ------------------

st.header("🌐 Servidor para Recepción Remota de Datos")

# Variables globales
nombre_medicion = st.text_input("📝 ¿Qué estás midiendo?", "material_desconocido")
iniciar_servidor = st.button("🚀 Iniciar servidor y descargar las telemetrías")

# Definiciones necesarias
expected_channels = ['R', 'S', 'T', 'U', 'V', 'W', 'G', 'H', 'I', 'J', 'K', 'L', 'A', 'B', 'C', 'D', 'E', 'F']
CSV_FILE = f'espectroscopia_{nombre_medicion.lower().strip()}.csv'

# Flask App (creada pero no ejecutada todavía)
flask_app = Flask(__name__)

@flask_app.route('/api/as7265x-data', methods=['POST'])
def recibir_datos_as7265x():
    data = request.get_json()
    if not all(channel in data for channel in expected_channels):
        return jsonify({"error": "Faltan algunos canales"}), 400

    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    row_data = {"timestamp": timestamp}
    for channel in expected_channels:
        row_data[channel] = data.get(channel, 0)

    # Crear archivo si no existe
    if not os.path.exists(CSV_FILE):
        with open(CSV_FILE, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=['timestamp'] + expected_channels)
            writer.writeheader()

    # Escribir datos
    with open(CSV_FILE, 'a', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['timestamp'] + expected_channels)
        writer.writerow(row_data)

    return jsonify({"mensaje": "Datos recibidos correctamente"}), 200

# Función para ejecutar Flask
def run_flask():
    flask_app.run(host='0.0.0.0', port=5000)

# Iniciar servidor y ngrok
if iniciar_servidor:
    st.write("🔄 Iniciando servidor Flask en segundo plano...")
    flask_thread = threading.Thread(target=run_flask)
    flask_thread.daemon = True
    flask_thread.start()

    try:
        subprocess.Popen([
            "powershell", "-Command",
            'Start-Process "ngrok" -ArgumentList "http", "--url=climbing-champion-werewolf.ngrok-free.app", "5000"'
        ])
        st.success("✅ Servidor Flask corriendo en http://localhost:5000")
        st.info("🌍 Ngrok está exponiendo el servidor públicamente.")
    except Exception as e:
        st.error(f"❌ Error iniciando ngrok: {e}")


# ------------------ Sección 2: Entrenamiento ------------------

st.header("📁 Cargar datos de entrenamiento")

archivos_entrenamiento = st.file_uploader("Sube uno o más CSV con datos etiquetados", accept_multiple_files=True, type="csv")

if archivos_entrenamiento:
    dataframes = []
    for archivo in archivos_entrenamiento:
        nombre = archivo.name
        if not nombre.startswith("espectroscopia_"):
            st.warning(f"⚠️ {nombre} no sigue el formato esperado (espectroscopia_[MATERIAL].csv)")
            continue
        etiqueta = nombre.replace("espectroscopia_", "").replace(".csv", "").lower()
        df = pd.read_csv(archivo)
        if all(col in df.columns for col in CANALES):
            df['material'] = etiqueta
            dataframes.append(df)
        else:
            st.error(f"❌ {nombre} no contiene las columnas esperadas.")
    
    if dataframes:
        df_total = pd.concat(dataframes, ignore_index=True)
        st.success(f"✅ Se cargaron {len(df_total)} muestras.")

        if st.button("🧠 Entrenar modelo"):
            X = df_total[CANALES]
            y = df_total['material']

            scaler = StandardScaler()
            X_scaled = scaler.fit_transform(X)

            X_train, X_test, y_train, y_test = train_test_split(X_scaled, y, test_size=0.2, stratify=y, random_state=42)

            clf = RandomForestClassifier()
            clf.fit(X_train, y_train)

            # Guardar en session_state
            st.session_state.modelo = clf
            st.session_state.escalador = scaler
            st.session_state.modelo_entrenado = True

            y_pred = clf.predict(X_test)
            reporte = classification_report(y_test, y_pred, output_dict=False)
            st.text("📊 Reporte del modelo:")
            st.text(reporte)

# ------------------ Sección 3: Guardar y cargar modelos ------------------

st.header("💾 Guardar o cargar modelo entrenado")

# Guardar modelo actual
if st.session_state.modelo_entrenado:
    nombre_guardado = st.text_input("Nombre para guardar el modelo:", value="modelo_nuevo")

    if st.button("Guardar modelo"):
        ruta = os.path.join(MODELOS_DIR, f"{nombre_guardado}.pkl")
        with open(ruta, "wb") as f:
            joblib.dump({
                "modelo": st.session_state.modelo,
                "escalador": st.session_state.escalador
            }, f)
        st.success(f"✅ Modelo guardado como '{nombre_guardado}.pkl'")
else:
    st.info("Entrena un modelo primero para poder guardarlo.")

# Cargar modelo
st.subheader("📂 Cargar modelo previamente guardado")
modelos_disponibles = [f for f in os.listdir(MODELOS_DIR) if f.endswith(".pkl")]

if modelos_disponibles:
    modelo_seleccionado = st.selectbox("Selecciona un modelo:", modelos_disponibles)

    if st.button("Cargar modelo seleccionado"):
        with open(os.path.join(MODELOS_DIR, modelo_seleccionado), "rb") as f:
            datos = joblib.load(f)
            st.session_state.modelo = datos["modelo"]
            st.session_state.escalador = datos["escalador"]
            st.session_state.modelo_entrenado = True
        st.success(f"📦 Modelo '{modelo_seleccionado}' cargado correctamente.")
else:
    st.info("No hay modelos guardados todavía.")

# ------------------ Sección 4: Predicción ------------------

st.header("🔎 Analizar nueva medición")

archivo_medicion = st.file_uploader("Sube un archivo CSV con una medición para analizar", type="csv", key="pred")

if archivo_medicion:
    df = pd.read_csv(archivo_medicion)
    if all(col in df.columns for col in CANALES):
        espectro = df[CANALES].mean()
        st.subheader("📈 Espectro Promedio")
        fig, ax = plt.subplots()
        ax.plot(CANALES, espectro.values, marker='o')
        ax.set_ylabel("Intensidad")
        ax.set_title("Espectro (A–W)")
        st.pyplot(fig)

        # Cargar modelo desde session_state
        modelo = st.session_state.modelo
        escalador = st.session_state.escalador

        if modelo and escalador:
            X = df[CANALES]
            X_scaled = escalador.transform(X)
            pred = modelo.predict(X_scaled)
            probas = modelo.predict_proba(X_scaled)
            clases = modelo.classes_

            pred_clase = pred[0]
            pred_proba = probas[0]
            max_proba = pred_proba.max()

            st.success(f"📦 Material identificado: **{pred_clase.capitalize()}** (confianza: {max_proba:.1%})")

            if max_proba < 0.6:
                st.warning("⚠️ El modelo no está seguro de esta predicción. Podría tratarse de un material no conocido.")

            st.subheader("🔍 Probabilidades por clase:")
            df_probas = pd.DataFrame({
                "Material": clases,
                "Probabilidad": pred_proba
            }).sort_values(by="Probabilidad", ascending=False).reset_index(drop=True)

            st.dataframe(df_probas.style.format({"Probabilidad": "{:.1%}"}))

            if len(pred) > 1:
                st.write("Distribución de predicciones:")
                st.bar_chart(pd.Series(pred).value_counts())
        else:
            st.error("⚠️ No hay modelo cargado o entrenado.")
    else:
        st.error("❌ El archivo no contiene las columnas A–W.")

# ------------------ Sección 5: Comparación ------------------

st.header("📊 Comparación de espectros promedio")

carpeta = "datos_materiales"
if os.path.exists(carpeta):
    archivos = [f for f in os.listdir(carpeta) if f.endswith(".csv")]
    materiales_disponibles = {
        archivo.split("espectroscopia_")[-1].replace(".csv", ""): archivo
        for archivo in archivos
    }

    seleccion = st.multiselect(
        "Selecciona los materiales que quieres comparar:",
        list(materiales_disponibles.keys()),
        default=list(materiales_disponibles.keys())[:2]
    )

    if seleccion:
        fig, ax = plt.subplots(figsize=(10, 5))
        for material in seleccion:
            archivo = materiales_disponibles[material]
            df = pd.read_csv(os.path.join(carpeta, archivo))
            promedio = df[CANALES].mean()
            ax.plot(CANALES, promedio, label=material)

        ax.set_title("Espectros promedio de materiales seleccionados")
        ax.set_xlabel("Canal espectral")
        ax.set_ylabel("Intensidad")
        ax.legend()
        ax.grid(True)

        st.pyplot(fig)
    else:
        st.info("Selecciona al menos un material para visualizar su espectro.")
else:
    st.warning("⚠️ La carpeta 'datos_materiales' no existe.")


