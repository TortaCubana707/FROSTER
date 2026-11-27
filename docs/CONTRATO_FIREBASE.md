# Contrato de datos de FROSTER en Firebase Realtime Database

Proyecto `sqliteemi`, base `https://sqliteemi-default-rtdb.firebaseio.com`. Todo vive bajo el nodo
**`froster/`**. El ESP32, la app Android y el tablero React leen y escriben estas mismas rutas:
si algo cambia aquí, cambia en los tres.

```
froster/
├── lecturas/      ESP32 escribe · app y web leen
├── estado/        ESP32 escribe · app y web leen
├── config/        app y web escriben · ESP32 lee (stream)
├── ack/           ESP32 escribe · app y web leen (confirmación de comandos)
├── dispositivo/   ESP32 escribe · app y web leen
└── eventos/       ESP32 agrega (push) · app y web leen
```

## Por qué está separado así

- **Un nodo por quién escribe.** El ESP32 nunca escribe en `config/` (salvo los valores de fábrica la
  primera vez o las órdenes por USB) y la app nunca escribe en `lecturas/` ni `estado/`. Así nadie
  pisa los datos del otro.
- **Desnormalizado.** Cada pantalla lee un nodo completo en una sola consulta; no hay que juntar datos
  de varias rutas. `estado/setpoint` repite `config/setpoint` a propósito: `config` es lo que se
  *pidió* y `estado` lo que el ESP32 *está usando*.
- **Tipos reales.** Los números se guardan como números, los interruptores como booleanos y solo los
  textos como cadenas. Nunca `"24.5"` entre comillas.
- **Sin tráfico de más.** El ESP32 sube `lecturas/` y `estado/` solo cuando algo cambia de verdad,
  más un latido cada 10 s. Los comandos llegan al ESP32 por un *stream*, no preguntando cada rato.

## `lecturas/` (ESP32 → todos)

| Clave | Tipo | Unidad | Qué es |
|---|---|---|---|
| `ambiente` | número | °C | Temperatura de la cámara (DHT22) |
| `humedad` | número | % | Humedad de la cámara (DHT22) |
| `punto_rocio` | número | °C | Bajo esta temperatura el aire condensa |
| `placa_fria` | número | °C | Bloque de aluminio frío (DS18B20) |
| `fuga_pct` | número | % | Humedad junto a la bomba: 0 seco, 100 mojado |
| `pendiente` | número | °C/min | Tendencia de la cámara |
| `prediccion` | número | °C | Cámara prevista a 2 min |
| `ts` | número | ms desde 1970 | Hora (NTP) en que el ESP32 midió |

Se sube si alguna lectura cambia más que su umbral (0.2 °C, 1 % de humedad, 2 % de fuga…) o cada
10 s. Un sensor sin lectura **no aparece** (no se manda 0).

## `estado/` (ESP32 → todos)

| Clave | Tipo | Qué es |
|---|---|---|
| `nivel` | número 1–3 | 1 bomba y ventiladores · 2 una celda · 3 dos celdas |
| `celda_a`, `celda_b`, `bomba` | booleano | Salidas encendidas |
| `pwm_fans` | número 0–100 | Velocidad de los ventiladores en % |
| `setpoint` | número °C | Objetivo que está usando el ESP32 |
| `modo` | texto | `auto` · `manual` · `off` |
| `motivo` | texto | Por qué decidió así (para mostrar al usuario) |
| `alerta` | texto | Vacío si no hay alerta |
| `alerta_id` | número | 0 ninguna · 1 congelamiento · 2 sin hielo o bomba · 3 falla de sensor · 4 fuga de agua |
| `condensa` | booleano | El bloque está bajo el punto de rocío |
| `carga_alta` | booleano | Se avisó carga alta |
| `uptime_s` | número | Segundos desde que arrancó el ESP32 |

**¿El ESP32 está vivo?** `uptime_s` cambia al menos cada 10 s. Si no cambia en 15 s, la app y la web
deben decir "sin datos del ESP32" y no presentar los valores como actuales.

**Usar `alerta_id` para la lógica** (colores, sonido, notificaciones) y `alerta` solo para mostrar.

## `config/` (app y web → ESP32)

| Clave | Tipo | Rango | Qué hace |
|---|---|---|---|
| `setpoint` | número | 15–40 | Objetivo de la cámara en °C (umbral dinámico) |
| `modo` | texto | `auto`, `manual`, `off` | Modo de trabajo |
| `manual_a`, `manual_b` | booleano | | Celdas en modo manual |
| `carga_alta` | booleano | | Avisar que viene carga |
| `cmd_id` | número | | Identificador del comando (ver abajo) |

**Cómo mandar un comando:** escribir con `update()` solo las claves que cambian **más** `cmd_id` con
la hora actual en ms (`Date.now()` en JS, `System.currentTimeMillis()` en Java):

```js
update(ref(db, "froster/config"), { setpoint: 26, cmd_id: Date.now() });
```

## `ack/` (ESP32 → todos)

Cuando el ESP32 aplica un comando que trae `cmd_id`, responde aquí:

| Clave | Tipo | Qué es |
|---|---|---|
| `cmd_id` | número | El mismo `cmd_id` que se mandó |
| `recibido_ms` | número | Hora NTP del ESP32 al recibirlo |
| `aplicado_en_ms` | número | Lo que tardó el ESP32 en confirmarlo |

Quien mandó el comando escucha `ack/`; cuando llega su `cmd_id`, el comando se aplicó. Lo que pasó
entre escribir y ver el `ack` es la **latencia de ida y vuelta** (rúbrica de la E2: < 500 ms). Sirve
también para mostrar "Comando confirmado" (Snackbar o toast).

## `dispositivo/` (ESP32 → todos, cada 30 s)

| Clave | Tipo | Qué es |
|---|---|---|
| `ip` | texto | IP de la placa en la red |
| `rssi` | número | Señal Wi-Fi en dBm |
| `caidas_wifi` | número | Veces que se perdió la red desde el arranque |
| `firmware` | texto | Versión del firmware |
| `ultima_conexion` | número | Hora del servidor de Firebase (ms) |

## `eventos/{id}` (ESP32 agrega)

Cada evento es un hijo con id automático (`push`), así nunca se sobrescriben.

| Clave | Tipo | Qué es |
|---|---|---|
| `tipo` | texto | `nivel` · `alerta` · `pico` |
| `texto` | texto | Descripción lista para mostrar |
| `nivel` | número | Nivel en ese momento |
| `camara` | número | Temperatura de la cámara en °C |
| `setpoint` | número | Objetivo en ese momento |
| `ts` | número | Hora del servidor (ms) |

`pico` = la cámara pasó el objetivo + 2.5 °C. Estos son los **picos de temperatura excedidos** que el
Proyecto 1 pide guardar en Room. Para leer los últimos: `orderByKey().limitToLast(20)`.

## Ejemplo completo

Ver [`firebase/muestra.json`](../firebase/muestra.json): es una muestra con la forma exacta del árbol.
Para la entrega de la E2 se usa la **exportación real** desde la consola de Firebase.
