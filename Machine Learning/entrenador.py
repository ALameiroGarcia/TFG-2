import os
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import classification_report
import joblib

# Ruta absoluta a la carpeta donde está este script
base_dir = os.path.dirname(os.path.abspath(__file__))
carpeta = os.path.join(base_dir, 'datos_materiales')  # Subcarpeta relativa

dataframes = []

for archivo in os.listdir(carpeta):
    if archivo.startswith('espectroscopia_') and archivo.endswith('.csv'):
        ruta = os.path.join(carpeta, archivo)

        # Extrae el nombre del material: espectroscopia_[MATERIAL].csv
        nombre_material = archivo.replace('espectroscopia_', '').replace('.csv', '').lower()

        df = pd.read_csv(ruta)
        df['material'] = nombre_material
        dataframes.append(df)

# Unir datos
df_total = pd.concat(dataframes, ignore_index=True)

# Separar características y etiquetas
X = df_total[['A','B','C','D','E','F','G','H','I','J','K','L','R','S','T','U','V','W']]
y = df_total['material']

# Preprocesamiento
scaler = StandardScaler()
X_scaled = scaler.fit_transform(X)

# Dividir
X_train, X_test, y_train, y_test = train_test_split(X_scaled, y, test_size=0.2, stratify=y, random_state=42)

# Entrenar
clf = RandomForestClassifier()
clf.fit(X_train, y_train)

# Evaluación
print("Evaluación del modelo:")
print(classification_report(y_test, clf.predict(X_test)))

# Guardar modelo y escalador en la misma carpeta que el script
joblib.dump(clf, os.path.join(base_dir, 'modelo_materiales.pkl'))
joblib.dump(scaler, os.path.join(base_dir, 'escalador.pkl'))
