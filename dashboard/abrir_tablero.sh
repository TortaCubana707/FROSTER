#!/bin/bash
# Abre el tablero de FROSTER en el navegador. Firebase solo permite iniciar sesión desde
# localhost o un dominio autorizado (no desde file://), por eso se sirve con Python.
cd "$(dirname "$0")"
python3 -m http.server 8080 --bind 127.0.0.1 >/dev/null 2>&1 &
sleep 1
xdg-open "http://localhost:8080/index.html" >/dev/null 2>&1
echo "Tablero en http://localhost:8080 — cierra esta terminal para apagarlo."
wait
