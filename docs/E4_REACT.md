# E4 · Tablero web en React (Kevin)

> Consigna: panel web con datos en vivo, indicadores visuales reactivos, mensajes en pantalla,
> alertas dinámicas y alarmas (visuales/sonoras) para cambios críticos o para confirmar comandos.
> Entregar: enlace al repositorio y capturas o URL del despliegue mostrando una alerta visible.

## Arrancar

```bash
cd web
cp .env.example .env.local      # datos públicos del proyecto de Firebase
npm install
npm run dev                     # abre http://localhost:5173
```

Si `npm install` avisa de scripts bloqueados de `esbuild`, correr `npm approve-scripts esbuild`.

## Lo que ya está hecho (y compila con `npm run build`)

| Requisito | Dónde |
|---|---|
| SDK modular de Firebase | `src/firebase.js` (`initializeApp`, `getAuth`, `getDatabase`) |
| Datos en vivo | `src/hooks/useNodo.js` (`onValue`) |
| Comandos concurrentes con la app | `src/hooks/useComando.js`: `config/` con `cmd_id`, espera `ack/` y devuelve los ms |
| Alerta visual | Banner que parpadea cuando `estado/alerta_id > 0` (`Tablero.jsx`) |
| Alarma sonora | `src/sonido.js` (Web Audio, sin archivos) al aparecer una alerta nueva |
| Mensajes en pantalla | `src/componentes/Avisos.jsx` (tipo toast): alertas, alerta resuelta, comando confirmado |
| ¿El ESP32 está vivo? | `src/hooks/useLatido.js`: "EN VIVO" / "SIN DATOS DEL ESP32" |
| Rutas y códigos | `src/contrato.js` (copia de `docs/CONTRATO_FIREBASE.md`) |

## Lo que falta (E4)

- [ ] **Maquetado** del tablero: más tarjetas, colores por nivel, diseño responsivo.
- [ ] Gráfica de la cámara y el bloque (por ejemplo con `recharts` o `chart.js`).
- [ ] Controles de **modo** y celdas en manual (usar `comando({ modo: "manual" })`, etc.).
- [ ] Indicadores de `celda_a`, `celda_b`, `bomba`, `pwm_fans` y `motivo`.
- [ ] Lista de eventos (`froster/eventos`, `limitToLast`), y latencia en pantalla.
- [ ] Desplegar en **Vercel** (raíz `web/`, comando `npm run build`, salida `dist/`, variables
      `VITE_FIREBASE_*`) o en **Firebase Hosting**. Agregar el dominio de Vercel en
      Firebase → Authentication → Settings → Authorized domains, o el login fallará.
- [ ] Capturas con una alerta visible (por ejemplo, mojar el sensor de fuga).

El tablero viejo (`dashboard/index.html`) ya tiene gráfica, eventos y medidor de latencia: sirve
de referencia para lo que falta.
