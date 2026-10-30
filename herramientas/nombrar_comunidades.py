"""Pone nombres en espanol a las comunidades del grafo de graphify de FROSTER.

graphify solo sabe nombrarlas con una API de IA; sin ella deja "Community N". Aqui se
nombran por su contenido: el archivo que domina la comunidad y su nodo principal. Escribe
.graphify_labels.json y su firma .sig, que `graphify cluster-only` reutiliza.

    <python de graphify> nombrar_comunidades.py <carpeta graphify-out>
"""
import collections
import json
import sys
from pathlib import Path

from graphify.cluster import community_member_sigs

POR_ARCHIVO = {
    "app_android/RefrigeracionActivity.java": "App Android: pantalla de refrigeración",
    "tablero/tablero.js": "Tablero web en Firebase",
    "animacion/froster_escena.py": "Animación en Blender: escena",
    "animacion/armar_video.py": "Video y guía PDF",
    "animacion/render_video.sh": "Script: render del video",
    "animacion/render_resto.sh": "Script: render repartido en 3 tarjetas",
    "pruebas/explorar.cpp": "Simulación: barrido de cargas",
    "pruebas/probar.sh": "Script: correr las pruebas",
    "herramientas/actualizar_grafo.sh": "Script: regenerar este grafo",
    "herramientas/gen_guia.py": "Video y guía PDF",
    "herramientas/nombrar_comunidades.py": "Script: nombres de este grafo",
    "app_android/froster/AsistenteActivity.java": "App: asistente de voz FROSTER",
    "app_android/froster/ProcesadorFroster.java": "App: comandos y consultas por voz",
    "app_android/froster/ProcesadorFrosterTest.java": "App: pruebas del asistente",
    "app_android/froster/ChatFroster.java": "App: IA de Groq",
    "app_android/froster/EstadoFroster.java": "App: datos en vivo del ESP32",
    "app_android/froster/voz/Voz.java": "App: voz de FROSTER",
    "app_android/froster/voz/ReconocedorVoz.java": "App: reconocimiento de voz",
    "app_android/froster/util/Http.java": "App: utilidades",
    "app_android/froster/util/Hilos.java": "App: utilidades",
    "app_android/froster/util/Texto.java": "App: utilidades",
    "diagramas/generar_diagramas.py": "Diagramas de conexión (Excalidraw)",
    "tablero/abrir_tablero.sh": "Script: abrir el tablero",
}
# Archivos con varias comunidades: se distinguen por el nodo que contienen
POR_NODO = [
    ("pruebas/probar_logica.cpp", "Planta", "Simulación: modelo térmico de la cámara"),
    ("pruebas/probar_logica.cpp", "Corrida", "Pruebas de la lógica de control"),
    ("pruebas/probar_senales.cpp", "gestionarSenales()", "Firmware: LED RGB y buzzer"),
    ("firmware/FROSTER_Control.cpp", "gestionarSenales()", "Firmware: LED RGB y buzzer"),
    ("firmware/FROSTER_Control.cpp", "Control", "Firmware: memoria del controlador"),
    ("firmware/FROSTER_Control.cpp", "Salidas", "Firmware: salidas (celdas, bomba, alertas)"),
    ("firmware/FROSTER_Control.cpp", "decidir()", "Firmware: decisión, Firebase y pantalla"),
    ("firmware/FROSTER_Control.cpp", "atenderComandosSerie()", "Firmware: nube, OLED y órdenes por USB"),
    ("firmware/FROSTER_Control.cpp", "Entradas", "Firmware: entradas (sensores y órdenes)"),
    ("firmware/FROSTER_Control.cpp", "tocar()", "Firmware: melodías del buzzer"),
    ("pruebas/probar_senales.cpp", "sonidos()", "Pruebas del LED y el buzzer"),
    ("pruebas/probar_logica.cpp", "volcado()", "Pruebas: arnés de simulación"),
    ("pruebas/probar_logica.cpp", "Registro", "Pruebas: registro de cada paso"),
]


def archivo(nodo):
    """Ruta del archivo relativa a grafo/fuente/ (graphify la guarda asi o absoluta)."""
    ruta = (nodo.get("source_file") or "").replace("\\", "/")
    if "/fuente/" in ruta:
        ruta = ruta.split("/fuente/")[-1]
    elif ruta.startswith("fuente/"):
        ruta = ruta[len("fuente/"):]
    return ruta if "/" in ruta else ""


def nombrar(nodos):
    archivos = collections.Counter(a for a in map(archivo, nodos) if a)
    etiquetas = {n.get("label") for n in nodos}
    if not archivos:
        return None
    dominante = archivos.most_common(1)[0][0]
    for arch, nodo, nombre in POR_NODO:
        if arch in archivos and nodo in etiquetas:
            return nombre
    return POR_ARCHIVO.get(dominante, dominante)


def main(salida):
    salida = Path(salida)
    grafo = json.loads((salida / "graph.json").read_text(encoding="utf-8"))
    por_comunidad = collections.defaultdict(list)
    for n in grafo["nodes"]:
        if n.get("community") is not None:
            por_comunidad[int(n["community"])].append(n)

    nombres, usados = {}, collections.Counter()
    for cid in sorted(por_comunidad):
        nombre = nombrar(por_comunidad[cid]) or "Librería de Android (externa)"
        usados[nombre] += 1
        nombres[cid] = nombre if usados[nombre] == 1 else f"{nombre} ({usados[nombre]})"

    miembros = {cid: [n["id"] for n in ns] for cid, ns in por_comunidad.items()}
    (salida / ".graphify_labels.json").write_text(
        json.dumps({str(k): v for k, v in nombres.items()}, ensure_ascii=False, indent=2),
        encoding="utf-8")
    (salida / ".graphify_labels.json.sig").write_text(
        json.dumps({str(k): v for k, v in community_member_sigs(miembros).items()}, indent=2),
        encoding="utf-8")
    for cid, nombre in nombres.items():
        print(f"  {cid:2d}  {len(miembros[cid]):3d} nodos  {nombre}")


if __name__ == "__main__":
    main(sys.argv[1])
