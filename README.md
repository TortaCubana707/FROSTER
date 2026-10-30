# FROSTER — enfriamiento termoeléctrico inteligente

Módulo de enfriamiento con dos celdas Peltier, controlado por un ESP32 en tres niveles, con
telemetría en Firebase, tablero web y app Android. Se prueba en una **cámara de pruebas**:
un foco incandescente amarillo simula el calor de una computadora y el módulo tiene que sacarlo.

> Estado al 29 oct 2026: firmware cargado en el ESP32 y probado con el hardware real. Se conecta
> al Wi-Fi, publica en Firebase, la LCD, la OLED y el LED RGB funcionan, y en modo manual las
> celdas enfriaron la cámara 0.8 °C en 2.5 min. En la computadora pasan 52 verificaciones de
> control y las de LED y buzzer. Fotos del prototipo en [evidencias/E1](evidencias/E1) y video de 1 minuto en
> [video_e1/FROSTER_E1_video.mp4](video_e1/FROSTER_E1_video.mp4) (animación + fotos reales + voz).

La guía para armarlo paso a paso está en
[FROSTER_Guia_Conexiones.md](FROSTER_Guia_Conexiones.md).

---

## Cómo funciona

```
 cámara de focos ──(aire caliente)──► [ventilador] ► tubo de aluminio frío ► [ventilador] ──(aire frío)──► cámara
                                                         ▲            ▲
                                                     celda A      celda B      (cara fría contra el tubo)
                                                         ▼            ▼
                                                  waterblock A ► waterblock B   (cara caliente)
                                                         ▲            │
                                     bomba de diafragma ─┘            └─► cubeta con agua y hielo
```

| Nivel | Celdas | Cuándo |
|---|---|---|
| 1 | Ninguna (solo bomba y ventiladores) | La cámara está bajo el objetivo (28 °C) |
| 2 | Una | La cámara pasa del objetivo |
| 3 | Las dos | La cámara pasa del objetivo + 2.5 °C |

El control decide con la temperatura **prevista a 2 minutos**, exige que un cambio se sostenga
20 s, respeta 60 s mínimos encendida y apagada por celda, reparte el desgaste entre las dos
celdas y ensancha solo su banda si detecta que está ciclando.

### Protecciones (por encima de cualquier nivel)

| Condición | Qué hace |
|---|---|
| Bloque frío bajo 0.5 °C | Apaga las celdas; vuelven al pasar de 6 °C |
| Celda encendida 3 min y el bloque no está 2 °C más frío que la cámara | Se acabó el hielo o se paró la bomba (deducido, sin sensor de agua): apaga las celdas; reintenta tras 5 min por cada corte |
| Un sensor sin lectura 10 s | Apaga las celdas |
| Arranque de la placa | 60 s de espera; la bomba arranca 15 s antes que las celdas y sigue 60 s después |

---

## Materiales

| Componente | Cant. | Uso |
|---|---|---|
| ESP32 DevKit v1 + base de expansión | 1 | Control. Jumper de la base en **5V** |
| Celda Peltier TEC1-12710 | 2 | A **3.0 A** cada una (~4 V) |
| Bloque de aluminio 60×60×100 | 1 | Tubo frío, hecho con las 4 barras de aletas |
| Waterblock 40×40 | 2 | Lado caliente de cada celda |
| Bomba de diafragma 12 V, 0.5 A | 1 | Circula el agua con hielo |
| Ventilador 12 V 0.14 A | 2 | En paralelo, a través del tubo |
| Ventilador 60×60×25 ACP06025S12M, 12 V 0.23 A | 1 | En paralelo con los otros dos |
| Fuente 12 V 5 A 60 W (Osoelectronics, NOM) | 1 | Todo lo de 12 V |
| Reductor XL4016 | 2 | Uno por celda, **CV 5.0 V / CC 3.0 A** |
| Módulo MOSFET D4184 | 3 | Celda A, celda B, bomba + ventiladores |
| DS18B20 con adaptador | 1 | Bloque frío (el del agua se quemó al conectarse al revés) |
| DHT22 | 1 | Temperatura y humedad de la cámara |
| Pantalla LCD 16×2 I2C | 1 | El módulo, el uso y la red: bloque, agua, rocío, horas de uso, WiFi |
| Pantalla OLED 0.96" I2C (SSD1306) | 1 | Lo que decide el control: nivel, cámara, celdas, gráfica y motivo |
| LED RGB + 3 resistencias de 220 Ω | 1 | Color por estado |
| Diodo 1N4007 | 1 | Protección de la bomba |
| Power bank o cargador USB | 1 | Alimenta el ESP32 por el USB-C de la base |

---

## Alimentación

| Rama | Voltaje | Corriente desde la fuente |
|---|---|---|
| XL4016 #1 → celda A (3.0 A, ~4 V) | 12 V | ≈ 1.3 A |
| XL4016 #2 → celda B (3.0 A, ~4 V) | 12 V | ≈ 1.3 A |
| D4184 #3 → bomba + 3 ventiladores | 12 V | 0.5 + 0.28 + 0.23 A |
| **Total** | | **≈ 3.6 A de 5 A** |

El ESP32 va aparte, por USB. **No se usa el jack DC de la base**: pasa por un regulador
lineal AMS1117 que a 12 V se sobrecalentaría. Los D4184 tienen optoacoplador, así que **no se
unen** la tierra de la fuente y la del ESP32.

Los D4184 de las celdas van **antes** del XL4016: cortan su IN−. El módulo necesita 6 V o más
en su `+` (después del reductor solo habría ~4 V), y el XL4016 mide la corriente por su OUT−,
así que la celda va directo a OUT+ / OUT− para que funcione el límite de 3 A.

---

## Pines

| GPIO | Va a |
|---|---|
| 4 | DHT22 |
| 5 | Libre (no hay termómetro en el agua) |
| 18 | DS18B20 del bloque frío, arriba |
| 21 / 22 | Pantalla LCD (SDA / SCL) |
| 16 / 17 | Pantalla OLED, en su propio bus I2C (SDA / SCL) |
| 25 | D4184 #1 — celda A |
| 26 | D4184 #2 — celda B |
| 27 | D4184 #3 — bomba y ventiladores |
| 32 / 33 / 13 | LED RGB (R / G / B) |
| 23 | Libre (antes buzzer) |

**Sobre el jumper en 5V:** los sensores quedan alimentados a 5 V y su cable de datos llega al
ESP32 a 5 V, aunque sus pines están especificados hasta 3.6 V (la base no convierte niveles).
Fue una decisión consciente; si un termómetro empieza a fallar, es lo primero que se revisa.

---

## Señales

| Estado | LED | Buzzer |
|---|---|---|
| Arrancando | Azul parpadeando | Melodía de 3 notas |
| Nivel 1 | Verde | 2 tonos que bajan (al bajar de nivel) |
| Nivel 2 | Cian | 2 tonos que suben |
| Nivel 3 | Azul | 3 tonos que suben |
| Esperando o pre-circulando | Amarillo | — |
| Congelamiento | Morado parpadeando | 3 pitidos agudos, cada minuto |
| Sin hielo | Naranja parpadeando | 2 pitidos largos, cada minuto |
| Falla de sensor | Rojo parpadeando | Tono grave intermitente, cada minuto |

Si el LED resulta ser de **ánodo común**, se cambia `LED_ANODO_COMUN` a `1` en el código. Si el buzzer
zumba o se calienta en silencio, se cambia `BUZZER_ACTIVO_BAJO` a `1`.

---

## Asistente de voz (app)

En la app CRUDSQLITE1, pantalla de refrigeración → **"Hablar con FROSTER"**. Se le pregunta con
la voz o por escrito y contesta en voz alta con voz propia (español de México, tono más grave):

- **Comandos locales**, sin internet: estado, temperaturas, nivel, alertas, hielo, humedad,
  objetivo ("pon el objetivo en 26", "sube el objetivo"), modo (automático, manual, apagado),
  celdas en manual y aviso de carga alta. Escriben en `tarjeta/refrigeracion/config`.
- **Preguntas libres** a la IA de Groq (`openai/gpt-oss-120b`), con los datos en vivo.
- Avisa en voz alta las alertas nuevas. Si el ESP32 no publica (su `uptime_s` no cambia),
  lo dice en vez de presentar datos viejos como actuales.
- La clave va en `local.properties` (`GROQ_API_KEY`), que no se sube a git. Código en
  `app/src/main/java/com/example/crudsqlite1/froster/`, 26 pruebas JUnit.

El buzzer se perdió; el sonido del sistema es esta voz. El firmware conserva el código del buzzer
por si se repone (GPIO 23).

## Por qué 3 A por celda

Calculado con el datasheet de la TEC1-12710 (Thermonamic: Umax 16 V, Imax 10.1 A, ΔTmax 70 °C,
1.25 Ω), con agua helada en el lado caliente y aire de la cámara a 30 °C:

| Corriente por celda | Frío total (2 celdas) | Bloque frío | Eficiencia (COP) |
|---|---|---|---|
| 2 A | 50 W | 10 °C | 4.7 |
| **3 A** | **61 W** | **5.5 °C** | **2.3** |
| 4 A | 67 W | 3 °C | 1.4 |
| 5 A | 73 W | se congela | 1.0 |

De 3 a 4 A se gana 13 % de frío por casi el doble de consumo. Sin hielo, a 3 A la celda pide
~4.6 V: por eso los XL4016 van en **CV 5.0 V** y es el límite de corriente el que manda.

Con agua helada, incluso el nivel 1 enfría ~24 W solo por conducción a través de las celdas
(sin hielo, ~8 W). Para una comparación justa, medir también sin hielo.

Son resultados de un modelo, no mediciones; las celdas genéricas suelen rendir 20–30 % menos.

Prueba real (Poco F6 en AnTuTu, cargando): la batería subió +2.5 °C con el módulo contra
+8.2 °C sin él.

---

## Firebase

Proyecto `sqliteemi`, cuenta del ESP32 `esp32@correo.com`.

```
tarjeta/refrigeracion/
  lecturas/   ambiente, humedad, punto_rocio, placa_fria, disipador_a, pendiente, prediccion
  estado/     nivel, celda_a, celda_b, bomba, motivo, alerta, setpoint, modo, online
  config/     setpoint (15–40), modo (auto | manual | off), manual_a, manual_b, carga_alta
```

El tablero (`dashboard/index.html`) y la app Android (CRUDSQLITE1, `RefrigeracionActivity`) leen
estas mismas rutas.

### Tablero en la computadora

`dashboard/abrir_tablero.sh` lo sirve en `http://localhost:8080` y abre el navegador (Firebase
no deja iniciar sesión desde `file://`). Se entra con la cuenta del ESP32; la contraseña no se
guarda. Muestra nivel, cámara con tendencia y predicción, objetivo (con − / +), bloque frío y
rocío, qué equipos están encendidos, el motivo de la decisión, la alerta y una gráfica de los
últimos 30 min. Dice **EN VIVO** solo si `uptime_s` cambió en los últimos 15 s. Con
`index.html?demo` muestra datos simulados, marcados como DEMO, para revisar el diseño.
La versión anterior quedó en `dashboard/index_v1_respaldo.html`.

---

## Archivos

En GitHub solo está lo vigente; la animación de Blender, el grafo y las versiones anteriores
se quedan en la computadora.

| Archivo | Estado |
|---|---|
| `firmware/FROSTER_Control/FROSTER_Control.ino` | **Vigente** |
| `FROSTER_Control_CODIGO.txt` | **Vigente**: el mismo código, para copiar y pegar |
| `firmware/FROSTER_Control/pruebas/probar.sh` | **Vigente**: prueba la lógica y las señales en la PC |
| `FROSTER_Guia_Conexiones.md` | **Vigente**: armado paso a paso |
| `diagramas/FROSTER_conexiones_control.png` y `_potencia.png` | **Vigentes**: diagramas de conexión (los `.excalidraw` se editan en excalidraw.com) |
| `diagramas/generar_diagramas.py` | **Vigente**: regenera los dos diagramas |
| `dashboard/index.html` y `abrir_tablero.sh` | **Vigente**: tablero en vivo |
| `video_e1/` (`gen_diapositivas.py`, `armar_video_e1.py`, `guion.txt`) | **Vigente**: arma el video de la E1; la voz es es-MX-DaliaNeural (edge-tts) |
| `herramientas/actualizar_grafo.sh` | **Vigente**: regenera el grafo de graphify |
| `blender/` (modelo, animación y scripts) | Vigente, aunque muestra la versión con focos de 15 y 25 W |
| `FROSTER_Esquema_Corregido.md` | Parcialmente superado (ver su nota inicial) |
| `firmware/FROSTER_Termico`, `FROSTER_Demo`, `FROSTER_Emilio*`, `Parte1_*` | Versiones anteriores |
| `FROSTER_Emilio_CODIGO.txt`, `PARTE1_*.txt` | Versiones anteriores |
| `FROSTER_Guia_de_Armado.pdf` | Superada: era para la fuente ATX y los eliminadores |

---

## Comandos

Antes de compilar, copiar `firmware/FROSTER_Control/secretos.ejemplo.h` como `secretos.h` y
llenar las credenciales de Firebase. `secretos.h` no se sube a GitHub.

Compilar:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=min_spiffs" firmware/FROSTER_Control
```

Probar la lógica y las señales en la PC:

```bash
firmware/FROSTER_Control/pruebas/probar.sh
```

Órdenes por el Monitor Serie (115200, fin de línea "Nueva línea"); también se guardan en Firebase:

| Orden | Hace |
|---|---|
| `obj 23` | Objetivo en 23 °C (15–40) |
| `modo auto` / `modo manual` / `modo off` | Cambia el modo |
| `a 1` / `b 0` | Enciende o apaga una celda (solo en manual) |
| `carga 1` / `carga 0` | Aviso de carga alta |

Regenerar el grafo de graphify:

```bash
herramientas/actualizar_grafo.sh
```

Al subir desde el IDE de Arduino: **Herramientas → Partition Scheme → Minimal SPIFFS**. Con la
partición de fábrica no cabe.
