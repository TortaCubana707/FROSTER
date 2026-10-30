#!/bin/bash
# Regenera el grafo de graphify de FROSTER con las fuentes VIGENTES del proyecto.
#
# graphify no reconoce .ino ni el JavaScript metido en un .html, asi que primero se copian
# las fuentes a grafo/fuente/ con una extension que si entiende (.ino -> .cpp, el <script>
# del tablero -> .js). Las versiones anteriores del firmware NO entran, para que el grafo
# muestre el sistema actual y no la historia.
#
# Resultado: grafo/graphify-out/graph.html (abrir en el navegador).
set -e
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
FUENTE="$RAIZ/grafo/fuente"
APP="$HOME/AndroidStudioProjects/CRUDSQLITE1/app/src/main/java/com/example/crudsqlite1/RefrigeracionActivity.java"

rm -rf "$FUENTE"
mkdir -p "$FUENTE"/{firmware,pruebas,tablero,herramientas,animacion,app_android}

# Firmware vigente
cp "$RAIZ/firmware/FROSTER_Control/FROSTER_Control.ino" "$FUENTE/firmware/FROSTER_Control.cpp"

# Pruebas en la PC
cp "$RAIZ/firmware/FROSTER_Control/pruebas/"*.cpp "$FUENTE/pruebas/"
cp "$RAIZ/firmware/FROSTER_Control/pruebas/probar.sh" "$FUENTE/pruebas/"

# Tablero web: solo el <script> propio (los de Firebase y Chart.js vienen de CDN)
python3 - "$RAIZ/dashboard/index.html" "$FUENTE/tablero/tablero.js" <<'PY'
import re, sys
html = open(sys.argv[1], encoding="utf-8").read()
propios = re.findall(r"<script>(.*?)</script>", html, re.S)
open(sys.argv[2], "w", encoding="utf-8").write("\n\n".join(propios))
PY

# Herramientas y animacion
cp "$RAIZ/herramientas/gen_guia.py" "$FUENTE/herramientas/" 2>/dev/null || true   # nombrar_comunidades.py es del grafo, no del proyecto
cp "$RAIZ/herramientas/actualizar_grafo.sh" "$FUENTE/herramientas/"
cp "$RAIZ/blender/"*.py "$RAIZ/blender/"*.sh "$FUENTE/animacion/"

# App Android (pantalla de refrigeracion de CRUDSQLITE1 y el asistente de voz FROSTER)
[ -f "$APP" ] && cp "$APP" "$FUENTE/app_android/"
APP_FROSTER="$(dirname "$APP")/froster"
[ -d "$APP_FROSTER" ] && cp -r "$APP_FROSTER" "$FUENTE/app_android/"
PRUEBA_APP="$HOME/AndroidStudioProjects/CRUDSQLITE1/app/src/test/java/com/example/crudsqlite1/froster/ProcesadorFrosterTest.java"
[ -f "$PRUEBA_APP" ] && cp "$PRUEBA_APP" "$FUENTE/app_android/froster/"

# Diagramas de conexion y lanzador del tablero
mkdir -p "$FUENTE/diagramas"
cp "$RAIZ/diagramas/generar_diagramas.py" "$FUENTE/diagramas/"
cp "$RAIZ/dashboard/abrir_tablero.sh" "$FUENTE/tablero/"

cd "$RAIZ/grafo"
graphify extract fuente --code-only --out "$RAIZ/grafo" --force
graphify cluster-only "$RAIZ/grafo" --no-label

# Nombres de las comunidades en espanol (graphify solo los pone con una API de IA)
PY_GRAPHIFY="$(head -1 "$(command -v graphify)" | sed 's/^#!//')"
"$PY_GRAPHIFY" "$RAIZ/herramientas/nombrar_comunidades.py" "$RAIZ/grafo/graphify-out"
graphify cluster-only "$RAIZ/grafo"

# La pagina carga vis-network desde internet; se deja una copia local para verla sin conexion
VIS="https://unpkg.com/vis-network@9.1.6/standalone/umd/vis-network.min.js"
cd "$RAIZ/grafo/graphify-out"
[ -s vis-network.min.js ] || curl -sSfL -o vis-network.min.js "$VIS" || echo "Sin internet: graph.html usara la copia en linea"
[ -s vis-network.min.js ] && sed -i "s|$VIS|vis-network.min.js|" graph.html
echo
echo "Grafo listo: $RAIZ/grafo/graphify-out/graph.html"
