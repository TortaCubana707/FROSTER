/* =========================================
 * PROYECTO ESP32 - EMILIO
 * FROSTER: CONTROL INTELIGENTE DE 3 NIVELES
 * WiFiManager + Firebase + LCD + OLED + LED RGB + buzzer + 2 termometros + 2 celdas Peltier
 *
 * NIVELES
 *   1  solo bomba y ventilador (celdas apagadas)
 *   2  una celda
 *   3  las dos celdas
 *
 * QUE LO HACE INTELIGENTE
 *   1. Anticipa: decide con la temperatura PREVISTA a 2 minutos (la actual mas
 *      su tendencia). Sube de nivel antes de pasarse y baja antes de enfriar
 *      de mas.
 *   2. Histeresis y confirmacion: sube y baja con umbrales distintos y un
 *      cambio debe sostenerse 20 s, asi un pico de ruido no enciende nada.
 *      Si detecta ciclado rapido, ensancha sola la banda de bajada.
 *   3. Cuida las celdas: 60 s minimo encendida y 60 s minimo apagada. La bomba
 *      arranca 15 s antes que ellas y se apaga 60 s despues.
 *   4. Reparte el desgaste: en el nivel 2 trabaja la celda con menos horas.
 *   5. Se defiende: congelamiento del bloque, agua sin hielo o un sensor
 *      muerto apagan las celdas aunque la temperatura pida frio.
 *   6. Explica cada decision (motivo) en pantalla, por Serial y en Firebase,
 *      y la avisa con el LED RGB (color por estado) y el buzzer (sonido por evento).
 *   7. El control NO depende de internet: vive en su propia tarea.
 *
 * SIN BLOQUEOS
 *   No hay ni un delay(): el loop() reparte su trabajo con millis() (red cada
 *   3 s, LCD cada 3 s, OLED cada 1 s, una nota del buzzer cuando toca) y la
 *   tarea de control corre cada 2 s en su propio hilo de FreeRTOS. Si se cae
 *   el WiFi, vigilarWiFi() lo reintenta cada 10 s sin detener nada.
 *
 * ARCHIVOS (pestanas del IDE de Arduino)
 *   FROSTER_Control.ino  configuracion, logica de decision, setup() y loop()
 *   control.ino          tarea de control: leer sensores, decidir y mover salidas
 *   nube.ino             WiFi, Firebase y ordenes por USB
 *   pantallas.ino        LCD 16x2 y OLED
 *   senales.ino          LED RGB y buzzer
 *
 * CONEXIONES
 *   DHT22 (camara) ........ GPIO4         Pantalla LCD I2C ..... SDA 21, SCL 22
 *   DS18B20 bloque frio ... GPIO18 (dentro del bloque, congelamiento)
 *   DS18B20 waterblock .... no instalado (SENSOR_AGUA 0); el DHT22 mide la camara
 *   D4184 celda A ......... GPIO25        D4184 celda B ........ GPIO26
 *   D4184 bomba+ventiladores GPIO27 (PWM a 1 kHz: mas aire en cada nivel)
 *   LED RGB ............... R GPIO32, G GPIO33, B GPIO13 (220 ohm en cada color)
 *   Buzzer pasivo (modulo de 3 pines) S en GPIO23, VCC a V y GND a G del D23
 *   OLED 0.96 I2C ......... bus I2C PROPIO: SDA GPIO16, SCL GPIO17 (G y V de la fila D17).
 *                           Direccion 0x3C o 0x3D: se detecta sola. La LCD queda sola en 21/22
 *
 * SENALES
 *   OLED (que decide el control): nivel y camara en grande, celdas y bomba
 *           encendidas, tendencia, objetivo, grafica de la camara de los ultimos
 *           ~4 min y el motivo de la decision
 *   LCD 16x2 (el modulo, el uso y la red), una vista cada 3 s:
 *           bloque frio, agua, rocio y humedad | horas de uso de cada celda |
 *           WiFi, Firebase e IP | y las alertas intercaladas si hay
 *   LED:    arranque azul parpadeando | nivel 1 verde | nivel 2 cian | nivel 3 azul
 *           esperando amarillo | congelamiento morado, sin hielo naranja y
 *           falla de sensor rojo, los tres parpadeando
 *   Buzzer: melodia al arrancar | tonos que suben al subir de nivel y que bajan
 *           al bajar | alertas: 3 pitidos agudos (congelamiento), 2 largos (sin
 *           hielo), tono grave intermitente (sensor), como maximo 1 vez por minuto
 *
 * AL SUBIRLO: Herramientas -> Partition Scheme -> Minimal SPIFFS (1.9MB APP)
 * ========================================= */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* --- Credenciales Firebase: en secretos.h, que no se sube a GitHub --- */
#include "secretos.h"

/* --- Rutas (las mismas que ya lee la app y el tablero) --- */
#define RUTA_LECTURAS "tarjeta/refrigeracion/lecturas"
#define RUTA_ESTADO   "tarjeta/refrigeracion/estado"
#define RUTA_CONFIG   "tarjeta/refrigeracion/config"

/* --- Pines --- */
#define DHTPIN       4
#define DHTTYPE      DHT22
#define PIN_TUBO     18    // DS18B20 dentro del bloque frio (congelamiento)
#define PIN_AGUA     5     // DS18B20 del waterblock (solo si SENSOR_AGUA es 1)
/* 1 = hay termometro en el waterblock (GPIO5). 0 = no hay: la falta de hielo o de bomba
   se deduce del bloque frio (ver PROXY_* en la logica). */
#define SENSOR_AGUA  0
#define PIN_CELDA_A  25
#define PIN_CELDA_B  26
#define PIN_BOMBA    27    // bomba y los 3 ventiladores en un solo D4184, con PWM
#define PIN_LED_R    32
#define PIN_LED_G    33
#define PIN_LED_B    13
#define PIN_BUZZER   23
#define OLED_SDA     16    // segundo bus I2C (Wire1), solo para la OLED
#define OLED_SCL     17

/* Ventiladores con PWM. Comparten el D4184 #3 con la bomba; el ciclo de trabajo
   (0 a 255) sube con el nivel. El optoacoplador del D4184 (PC817) es lento:
   arriba de unos 2 kHz ya no alcanza a seguir la senal. */
#define PWM_VENT_HZ   1000
#define PWM_VENT_N1   180      // 70 %: solo bomba y ventiladores
#define PWM_VENT_N2   220      // 86 %: una celda
#define PWM_VENT_N3   255      // 100 %: las dos celdas

/* Calibracion: lo que se le suma a cada lectura. Se ajusta comparando contra un
   termometro de referencia (el DS18B20, en agua con hielo, debe marcar 0 C). */
#define CAL_CAMARA_C   0.0f
#define CAL_HUMEDAD    0.0f
#define CAL_BLOQUE_C   0.0f

#define WIFI_REINTENTO_MS 10000UL   // cada cuanto se reintenta la red si se cae

/* Tipo de LED RGB. 0 = catodo comun (pata larga a G)
                    1 = anodo comun  (pata larga al pin 3.3V) */
#define LED_ANODO_COMUN 0

/* Tipo de modulo de buzzer. 0 = el pin S va directo al buzzer (lo mas comun)
                             1 = trae transistor y se activa con senal BAJA
   Si con 0 el buzzer zumba o se calienta estando en silencio, cambia a 1. */
#define BUZZER_ACTIVO_BAJO 0

/* Una nota del buzzer. Va aqui arriba porque el IDE de Arduino declara las
   funciones al inicio del archivo y tocar() la necesita. */
struct Nota { uint16_t hz, ms; };            // hz 0 = silencio; ms 0 = fin

/* =========================================================================
 * LOGICA DE DECISION
 * C++ puro, sin una sola funcion de Arduino. Se prueba en la computadora con
 * escenarios simulados antes de tocar el hardware: recibe lo que miden los
 * sensores y devuelve que debe estar encendido y por que.
 * ========================================================================= */
// >>> LOGICA_INICIO
#include <math.h>
#include <stdint.h>

#define T_MIN_ENCENDIDA_MS  60000UL   // una celda no se apaga antes de esto
#define T_MIN_APAGADA_MS    60000UL   // ni se enciende antes de esto
#define T_CONFIRMA_MS       20000UL   // un cambio de nivel debe sostenerse 20 s
#define T_PRECIRC_MS        15000UL   // agua circulando antes de dar corriente
#define T_POSTCIRC_MS       60000UL   // y un rato despues de apagar
#define HORIZONTE_MIN       2.0f      // la prediccion mira 2 min adelante
#define CONGELA_AVISO_C     2.0f      // bloque frio: no subir de nivel
#define CONGELA_CORTE_C     0.5f      // bloque frio: celdas fuera
#define CONGELA_REARME_C    6.0f      // y no vuelven hasta calentar a esto
#define AGUA_AVISO_C       26.0f      // waterblock: prohibido el nivel 3
#define AGUA_CORTE_C       30.0f      // waterblock: celdas fuera
#define AGUA_REARME_C      24.0f      // vuelven al bajar a esto (o menos)
#define SENSOR_TOL_MS      10000UL    // un termometro sin lectura 10 s = falla
#define CAMARA_TOL_MS      30000UL    // el DHT22 es mas caprichoso: 30 s
/* Sin termometro en el agua, la falta de hielo o de bomba se deduce del bloque frio:
   con una celda encendida 3 min, el bloque debe estar al menos 2 C mas frio que la camara.
   Si no lo esta, la celda no puede sacar el calor (agua caliente o bomba parada). */
#define PROXY_ESPERA_MS    180000UL
#define PROXY_MARGEN_C      2.0f
#define PROXY_REARME_MS    300000UL   // 5 min apagadas por cada corte (maximo 30 min)

enum Modo : uint8_t { MODO_AUTO = 0, MODO_MANUAL = 1, MODO_APAGADO = 2 };
enum Motivo : uint8_t {
  M_REPOSO = 0, M_PRECIRC, M_SUBIDA, M_SOBRE, M_BAJO, M_ESPERA,
  M_CONGELA, M_SIN_HIELO, M_SENSOR, M_MANUAL, M_APAGADO, M_CARGA
};
enum Alerta : uint8_t { A_NINGUNA = 0, A_CONGELA, A_SIN_HIELO, A_SENSOR };

struct Entradas {
  float   tCamara, tTubo, tAgua;     // NAN si el sensor no contesta
  float   setpoint;
  uint8_t modo;
  bool    manualA, manualB, cargaAlta;
  bool    sinSensorAgua;             // true: no hay termometro en el waterblock
};

struct Salidas {
  bool    bomba, celdaA, celdaB;
  uint8_t nivel;                     // 1, 2 o 3
  uint8_t motivo, alerta;
  uint8_t fallas;                    // bit0 camara, bit1 bloque frio, bit2 agua
  bool    congelado, sinHielo, esperando;
  float   pendiente;                 // grados por minuto
  float   prediccion;                // grados a 2 min
};

struct Control {                     // memoria del controlador
  bool     iniciado;
  uint32_t tUlt, tUltMuestra, tBombaOn, tApagado;
  bool     bombaOn, apagadoPrev;
  bool     A, B;
  uint32_t tCambioA, tCambioB, msOnA, msOnB;
  uint8_t  lider;                    // 0 = A, 1 = B
  bool     filtroListo;
  float    tFilt, hist[18];
  uint8_t  nHist, hSig;
  bool     congelado, sinHielo;
  uint8_t  tripsAgua;
  uint32_t tOkCam, tOkTubo, tOkAgua;
  uint8_t  motivoNivel;
  uint8_t  candidato;                // nivel que se esta pidiendo (en celdas)
  uint32_t tAlgunaOn;                // desde cuando hay alguna celda encendida
  uint32_t tCorteProxy;              // ultimo corte deducido sin sensor de agua
  uint32_t tUltApagado;              // ultima vez que el control dejo todas las celdas apagadas
  uint8_t  ciclosRapidos;            // cuantas veces se prendio de nuevo en menos de 4 min
  uint32_t tCandidato;               // desde cuando se pide
};

static inline float _lim(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* Pendiente de la camara en grados por minuto, sobre las ultimas 18 muestras
   (una cada 5 s = 90 s). Con menos de 7 muestras todavia no se confia. */
static float _pendiente(const Control &c) {
  if (c.nHist < 7) return 0.0f;
  uint8_t nuevo = (c.hSig + 17) % 18;
  uint8_t viejo = (c.nHist < 18) ? 0 : c.hSig;
  float dtMin = (c.nHist - 1) * 5.0f / 60.0f;
  return (c.hist[nuevo] - c.hist[viejo]) / dtMin;
}

Salidas decidir(const Entradas &e, Control &c, uint32_t ahora) {
  Salidas s = {};
  const bool camOk  = !isnan(e.tCamara);
  const bool tuboOk = !isnan(e.tTubo);
  const bool aguaOk = !isnan(e.tAgua);

  if (!c.iniciado) {
    c.iniciado = true;
    c.tUlt = c.tUltMuestra = ahora;
    c.tOkCam = c.tOkTubo = c.tOkAgua = ahora;
    /* Al arrancar las celdas cuentan como "recien apagadas": esperan 60 s.
       Si la placa se reinicia en ciclo por un problema de alimentacion, las
       celdas nunca llegan a recibir corriente. */
    c.tCambioA = c.tCambioB = ahora;
  }
  uint32_t dt = ahora - c.tUlt; c.tUlt = ahora;
  if (c.A) c.msOnA += dt;
  if (c.B) c.msOnB += dt;

  // ---- validez de los sensores
  if (camOk)  c.tOkCam  = ahora;
  if (tuboOk) c.tOkTubo = ahora;
  if (aguaOk) c.tOkAgua = ahora;
  const bool fCam  = (ahora - c.tOkCam)  > CAMARA_TOL_MS;
  const bool fTubo = (ahora - c.tOkTubo) > SENSOR_TOL_MS;
  const bool fAgua = !e.sinSensorAgua && (ahora - c.tOkAgua) > SENSOR_TOL_MS;
  s.fallas = (fCam ? 1 : 0) | (fTubo ? 2 : 0) | (fAgua ? 4 : 0);

  // ---- filtro, tendencia y prediccion de la camara
  if (camOk) {
    if (!c.filtroListo) { c.tFilt = e.tCamara; c.filtroListo = true; }
    else c.tFilt += 0.25f * (e.tCamara - c.tFilt);
  }
  if (c.filtroListo && (ahora - c.tUltMuestra) >= 5000UL) {
    c.tUltMuestra = ahora;
    c.hist[c.hSig] = c.tFilt;
    c.hSig = (c.hSig + 1) % 18;
    if (c.nHist < 18) c.nHist++;
  }
  const float pend = _pendiente(c);
  const float pred = c.tFilt + _lim(HORIZONTE_MIN * pend, -2.0f, 3.0f);
  s.pendiente = pend;
  s.prediccion = pred;

  // ---- protecciones con memoria (se levantan con histeresis)
  if (tuboOk) {
    if (e.tTubo <= CONGELA_CORTE_C) c.congelado = true;
    else if (e.tTubo >= CONGELA_REARME_C) c.congelado = false;
  }
  const bool friaAlta = tuboOk && e.tTubo < CONGELA_AVISO_C;

  if (e.sinSensorAgua) {
    const bool alguna = c.A || c.B;
    if (alguna && tuboOk && camOk && (ahora - c.tAlgunaOn) >= PROXY_ESPERA_MS
        && e.tTubo > e.tCamara - PROXY_MARGEN_C) {
      if (!c.sinHielo && c.tripsAgua < 6) c.tripsAgua++;
      c.sinHielo = true;
      c.tCorteProxy = ahora;
    } else if (c.sinHielo && !alguna
               && (ahora - c.tCorteProxy) >= PROXY_REARME_MS * c.tripsAgua) {
      c.sinHielo = false;                 // reintenta; si sigue igual, el siguiente corte espera mas
    }
  } else if (aguaOk) {
    if (e.tAgua >= AGUA_CORTE_C) {
      if (!c.sinHielo && c.tripsAgua < 10) c.tripsAgua++;
      c.sinHielo = true;
    } else {
      /* Cada corte exige mas frio para volver: con el hielo derretido el agua
         se queda a 22 grados y, sin esto, las celdas arrancarian y se
         cortarian una y otra vez. Hay que ponerle hielo nuevo. */
      float rearme = AGUA_REARME_C - 3.0f * c.tripsAgua;
      if (rearme < 12.0f) rearme = 12.0f;
      if (e.tAgua <= rearme) c.sinHielo = false;
      if (e.tAgua <= 12.0f) c.tripsAgua = 0;
    }
  }
  const bool aguaAlta = !e.sinSensorAgua && aguaOk && e.tAgua >= AGUA_AVISO_C;
  s.congelado = c.congelado;
  s.sinHielo = c.sinHielo;

  // ---- bomba y ventilador: siempre, salvo apagado (y 60 s de post-circulacion)
  const bool apagado = (e.modo == MODO_APAGADO);
  if (apagado && !c.apagadoPrev) c.tApagado = ahora;
  c.apagadoPrev = apagado;
  const bool bombaDeseada = !apagado || (ahora - c.tApagado) < T_POSTCIRC_MS;
  if (bombaDeseada && !c.bombaOn) c.tBombaOn = ahora;
  c.bombaOn = bombaDeseada;
  s.bomba = c.bombaOn;
  const bool bombaLista = c.bombaOn && (ahora - c.tBombaOn) >= T_PRECIRC_MS;

  // ---- que se pide a las celdas
  bool seguridad = false;       // apagado forzado: ignora los tiempos minimos
  bool quiereA = false, quiereB = false;
  uint8_t alerta = A_NINGUNA, motivo = c.motivoNivel;

  const bool fallaSensor = fTubo || fAgua || (e.modo == MODO_AUTO && fCam);
  if (apagado)            { seguridad = true; motivo = M_APAGADO; }
  else if (fallaSensor)   { seguridad = true; motivo = M_SENSOR;    alerta = A_SENSOR; }
  else if (c.congelado)   { seguridad = true; motivo = M_CONGELA;   alerta = A_CONGELA; }
  else if (c.sinHielo)    { seguridad = true; motivo = M_SIN_HIELO; alerta = A_SIN_HIELO; }
  else if (e.modo == MODO_MANUAL) {
    quiereA = e.manualA; quiereB = e.manualB; motivo = M_MANUAL;
  } else {
    /* Si veniamos de una proteccion, apagado o manual, ese motivo ya no aplica */
    if (motivo == M_CONGELA || motivo == M_SIN_HIELO || motivo == M_SENSOR ||
        motivo == M_MANUAL || motivo == M_APAGADO || motivo == M_CARGA) motivo = M_REPOSO;
    const int celdas = (c.A ? 1 : 0) + (c.B ? 1 : 0);
    const float sube2 = e.cargaAlta ? e.setpoint - 0.5f : e.setpoint + 0.5f;
    const float sube3 = e.cargaAlta ? e.setpoint + 1.0f : e.setpoint + 2.5f;
    /* Banda adaptativa: si apagar las celdas fue seguido, en menos de 4 min,
       de volver a encenderlas, la carga es sostenida y soltar el frio tan
       pronto solo provoca ciclos. Cada ciclo rapido amplia 0.5 C la banda de
       bajada (maximo 2 C). Tras 15 min apagado vuelve a la banda normal. */
    const float extra = 0.5f * c.ciclosRapidos;
    const float baja3 = e.setpoint + 1.0f - extra;
    const float baja2 = e.setpoint - 1.0f - extra;
    /* Para BAJAR no basta la prediccion: la temperatura real tambien tiene
       que estar a menos de 1 C del umbral. Recien encendido el frio la
       pendiente es muy negativa y la prediccion "se pasa" aunque la camara
       siga arriba del objetivo; soltar ahi solo provoca que vuelva a subir. */
    const bool bajaA0 = (pred <= baja2) && (c.tFilt <= baja2 + 1.0f);
    const bool bajaA1 = (pred <= baja3) && (c.tFilt <= baja3 + 1.0f);
    int dese = celdas;
    switch (celdas) {
      case 0:  dese = (pred >= sube3) ? 2 : (pred >= sube2 ? 1 : 0); break;
      case 1:  dese = (pred >= sube3) ? 2 : (bajaA0 ? 0 : 1); break;
      default: dese = bajaA0 ? 0 : (bajaA1 ? 1 : 2); break;
    }
    if (e.cargaAlta && dese < 1 && pred > baja2) dese = 1;      // se sabe que viene carga
    if ((friaAlta || aguaAlta) && dese > celdas) dese = celdas; // cerca del limite: no subir
    if (friaAlta && dese > 1) dese = 1;

    /* Confirmacion: el DHT22 tiene ruido y un solo pico no debe encender una
       celda dos minutos. El cambio tiene que pedirse 20 s seguidos. */
    if (dese != celdas) {
      if (c.candidato != (uint8_t)dese) { c.candidato = (uint8_t)dese; c.tCandidato = ahora; }
      if ((ahora - c.tCandidato) < T_CONFIRMA_MS) dese = celdas;
    } else {
      c.candidato = (uint8_t)celdas;
    }

    if (dese > celdas) {
      const float umbral = (dese == 2) ? sube3 : sube2;
      motivo = e.cargaAlta ? M_CARGA : (c.tFilt >= umbral ? M_SOBRE : M_SUBIDA);
    } else if (dese < celdas) {
      motivo = M_BAJO;
    } else if (e.cargaAlta && celdas > 0) {
      motivo = M_CARGA;                   // sigue encendido porque se aviso carga
    }

    if (dese == 0) { quiereA = quiereB = false; }
    else if (dese == 2) { quiereA = quiereB = true; }
    else {
      if (celdas == 1) c.lider = c.A ? 0 : 1;
      else c.lider = (c.msOnA <= c.msOnB) ? 0 : 1;   // la que lleva menos horas
      quiereA = (c.lider == 0);
      quiereB = (c.lider == 1);
    }
  }
  c.motivoNivel = motivo;

  // ---- aplicar respetando tiempos y circulacion de agua
  bool nuevoA = c.A, nuevoB = c.B, espera = false;
  if (seguridad) {
    nuevoA = nuevoB = false;
  } else {
    if (c.A && !quiereA) { if (ahora - c.tCambioA >= T_MIN_ENCENDIDA_MS) nuevoA = false; else espera = true; }
    if (c.B && !quiereB) { if (ahora - c.tCambioB >= T_MIN_ENCENDIDA_MS) nuevoB = false; else espera = true; }
    if (friaAlta && c.A && c.B) {            // cerca de congelar: suelta al ayudante ya
      if (c.lider == 0) nuevoB = false; else nuevoA = false;
    }
    if (!c.A && quiereA) {
      if (bombaLista && (ahora - c.tCambioA) >= T_MIN_APAGADA_MS) nuevoA = true; else espera = true;
    }
    if (!c.B && quiereB) {
      if (bombaLista && (ahora - c.tCambioB) >= T_MIN_APAGADA_MS) nuevoB = true; else espera = true;
    }
  }
  const bool habiaCelda = c.A || c.B;
  if (nuevoA != c.A) { c.A = nuevoA; c.tCambioA = ahora; }
  if (nuevoB != c.B) { c.B = nuevoB; c.tCambioB = ahora; }
  if (!habiaCelda && (c.A || c.B)) c.tAlgunaOn = ahora;
  if (!seguridad) {                       // contar ciclos que provoco el propio control
    if (habiaCelda && !c.A && !c.B) c.tUltApagado = ahora;
    if (!habiaCelda && (c.A || c.B) && c.tUltApagado != 0) {
      if ((ahora - c.tUltApagado) < 240000UL && c.ciclosRapidos < 4) c.ciclosRapidos++;
    }
  }
  if (!c.A && !c.B && c.tUltApagado != 0 && (ahora - c.tUltApagado) > 900000UL) c.ciclosRapidos = 0;

  s.celdaA = c.A;
  s.celdaB = c.B;
  s.nivel = 1 + (c.A ? 1 : 0) + (c.B ? 1 : 0);
  s.alerta = alerta;
  s.esperando = espera;
  if (!seguridad && espera) motivo = (!bombaLista && (quiereA || quiereB)) ? M_PRECIRC : M_ESPERA;
  s.motivo = motivo;
  return s;
}
// <<< LOGICA_FIN

/* =========================================================================
 * HARDWARE Y NUBE
 * ========================================================================= */
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
WiFiManager wm;
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_SSD1306 oled(128, 64, &Wire1, -1);     // bus propio: no comparte con la LCD
bool hayOled = false;
OneWire busTubo(PIN_TUBO), busAgua(PIN_AGUA);
DallasTemperature termTubo(&busTubo), termAgua(&busAgua);
bool firebaseIniciado = false;
uint32_t gCaidasWiFi = 0;              // veces que se perdio la red desde el arranque
FirebaseJson pendienteConfig;          // ajustes pedidos por USB que faltan subir a Firebase
bool hayPendienteConfig = false;

/* Lo que comparten las dos tareas. Siempre se copia dentro de una seccion
   critica para no leer un dato a medio escribir. */
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Entradas gOrdenes = { NAN, NAN, NAN, 28.0f, MODO_AUTO, false, false, false, SENSOR_AGUA == 0 };
Salidas  gSalidas = {};
float gCam = NAN, gTubo = NAN, gAgua = NAN, gHum = NAN, gRocio = NAN;
uint32_t gMsOnA = 0, gMsOnB = 0;      // tiempo encendida de cada celda (para la LCD)
Control  ctl;      // solo lo toca la tarea de control

const char *MOTIVOS[] = { "Reposo", "Pre-circulando", "Subida rapida", "Sobre objetivo",
                          "Bajo objetivo", "Espera 60 s", "Congelamiento", "Sin hielo/bomba",
                          "Falla sensor", "Manual", "Apagado", "Carga alta" };
const char *ALERTAS[] = { "", "Congelamiento del bloque", "Sin hielo o bomba parada",
                          "Falla de sensor" };

static bool termometroOk(float t) { return !isnan(t) && t > -40.0f && t < 110.0f && t != 85.0f; }

static float puntoDeRocio(float t, float hr) {
  if (isnan(t) || isnan(hr) || hr < 1.0f) return NAN;
  const float a = 17.62f, b = 243.12f;
  float g = (a * t) / (b + t) + logf(hr / 100.0f);
  return (b * g) / (a - g);
}

/* =========================================================================
 * ARRANQUE Y BUCLE
 * ========================================================================= */
void setup() {
  /* Lo primero de todo: salidas en bajo. Si la placa se reinicia con las
     celdas calientes, arrancar apagada es la unica posicion segura. */
  pinMode(PIN_CELDA_A, OUTPUT); digitalWrite(PIN_CELDA_A, LOW);
  pinMode(PIN_CELDA_B, OUTPUT); digitalWrite(PIN_CELDA_B, LOW);
  pinMode(PIN_BOMBA,   OUTPUT); digitalWrite(PIN_BOMBA,   LOW);
  ledcAttach(PIN_BOMBA, PWM_VENT_HZ, 8);          // bomba y ventiladores con PWM
  ledcWrite(PIN_BOMBA, 0);
  iniciarSenales();

  Serial.begin(115200);
  Serial.println("\n=== FROSTER - control inteligente de 3 niveles ===");

  dht.begin();
  termTubo.begin(); termAgua.begin();
  termTubo.setResolution(11); termAgua.setResolution(11);
  termTubo.setWaitForConversion(false);          // la espera la hace la tarea
  termAgua.setWaitForConversion(false);
  Serial.printf("Termometro bloque frio (GPIO%d): %s\n", PIN_TUBO, termTubo.getDeviceCount() ? "detectado" : "NO APARECE");
#if SENSOR_AGUA
  Serial.printf("Termometro waterblock  (GPIO%d): %s\n", PIN_AGUA, termAgua.getDeviceCount() ? "detectado" : "NO APARECE");
#else
  Serial.println("Termometro waterblock: no se usa. Falta de hielo/bomba se deduce del bloque frio.");
#endif

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("FROSTER");
  lcd.setCursor(0, 1); lcd.print("Iniciando...");
  iniciarOled();


  /* El control arranca ANTES de buscar el WiFi: con o sin internet ya esta
     decidiendo. Prioridad 2 sobre el loop (1), asi nunca espera a la nube. */
  xTaskCreatePinnedToCore(tareaControl, "control", 6144, NULL, 2, NULL, 1);

  /* WiFiManager sin bloquear: si no conoce la red, abre el punto de acceso
     "ESP32_Emilio" y el resto del programa sigue trabajando. */
  WiFi.setAutoReconnect(true);
  wm.setConfigPortalBlocking(false);
  wm.setConnectTimeout(20);
  if (wm.autoConnect("ESP32_Emilio")) {
    Serial.print("Conectado con IP: ");
    Serial.println(WiFi.localIP());               // <-- esta linea es la evidencia
  } else {
    Serial.println("Sin red conocida: conectate a ESP32_Emilio desde el celular.");
  }
}

void loop() {
  atenderComandosSerie();
  static uint32_t tNube = 0, tPantalla = 0, tSerie = 0;
  uint32_t ahora = millis();

  wm.process();                                   // atiende el portal si esta abierto
  vigilarWiFi();                                  // reconexion autonoma
  if (WiFi.status() == WL_CONNECTED && !firebaseIniciado) iniciarFirebase();

  Salidas s; Entradas o; float cam, tubo, agua, hum, rocio; uint32_t msA, msB;
  portENTER_CRITICAL(&mux);
  s = gSalidas; o = gOrdenes;
  cam = gCam; tubo = gTubo; agua = gAgua; hum = gHum; rocio = gRocio;
  msA = gMsOnA; msB = gMsOnB;
  portEXIT_CRITICAL(&mux);

  if (ahora - tNube >= 3000UL) {
    tNube = ahora;
    if (firebaseIniciado && WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      // Primero se sube lo pedido por USB; si no, leerOrdenes() lo pisaria con lo viejo
      if (hayPendienteConfig && Firebase.updateNode(fbdo, RUTA_CONFIG, pendienteConfig)) {
        pendienteConfig.clear();
        hayPendienteConfig = false;
      }
      if (!hayPendienteConfig) leerOrdenes();
      publicar();
    }
  }

  if (ahora - tPantalla >= 3000UL) {
    tPantalla = ahora;
    pantalla(s, tubo, agua, hum, rocio, msA, msB);
  }

  if (ahora - tSerie >= 2000UL) {
    tSerie = ahora;
    Serial.printf("N%d | cam %.1f (%+.2f/min, prev %.1f) obj %.1f | bloque %.1f agua %.1f | "
                  "A:%s B:%s bomba:%s | %s%s%s\n",
                  s.nivel, cam, s.pendiente, s.prediccion, o.setpoint, tubo, agua,
                  s.celdaA ? "ON" : "--", s.celdaB ? "ON" : "--", s.bomba ? "ON" : "--",
                  MOTIVOS[s.motivo], s.alerta ? " | ALERTA: " : "", ALERTAS[s.alerta]);
  }

  registrarHistorial(cam);
  static uint32_t tOled = 0;
  if (ahora - tOled >= 1000UL && !sonando()) {           // no interrumpir una melodia:
    tOled = ahora;                                        // dibujar tarda ~0.1 s
    pantallaOled(s, o, cam);
  }
  gestionarSenales(s);                            // sin delay(): el loop nunca se detiene
}
