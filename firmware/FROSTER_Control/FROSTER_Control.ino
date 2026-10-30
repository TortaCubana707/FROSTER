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
 * CONEXIONES
 *   DHT22 (camara) ........ GPIO4         Pantalla LCD I2C ..... SDA 21, SCL 22
 *   DS18B20 bloque frio ... GPIO18 (arriba, congelamiento)
 *   DS18B20 waterblock .... no instalado (SENSOR_AGUA 0); el DHT22 mide la camara
 *   D4184 celda A ......... GPIO25        D4184 celda B ........ GPIO26
 *   D4184 bomba+ventiladores GPIO27
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
#define PIN_TUBO     18    // DS18B20 arriba, en el bloque frio (congelamiento)
#define PIN_AGUA     5     // DS18B20 del waterblock (solo si SENSOR_AGUA es 1)
/* 1 = hay termometro en el waterblock (GPIO5). 0 = no hay: la falta de hielo o de bomba
   se deduce del bloque frio (ver PROXY_* en la logica). */
#define SENSOR_AGUA  0
#define PIN_CELDA_A  25
#define PIN_CELDA_B  26
#define PIN_BOMBA    27    // bomba y los 3 ventiladores en un solo D4184
#define PIN_LED_R    32
#define PIN_LED_G    33
#define PIN_LED_B    13
#define PIN_BUZZER   23
#define OLED_SDA     16    // segundo bus I2C (Wire1), solo para la OLED
#define OLED_SCL     17

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

/* ---------------------------------------------------------------------------
 * TAREA DE CONTROL
 * Lee, decide y mueve las salidas cada 2 s, pase lo que pase con el WiFi o con
 * Firebase. Si internet se cae o la nube tarda, esto no se entera.
 * ------------------------------------------------------------------------- */
void tareaControl(void *arg) {
  for (;;) {
    uint32_t t0 = millis();

    termTubo.requestTemperatures();
    termAgua.requestTemperatures();
    float cam = dht.readTemperature();
    float hum = dht.readHumidity();
    vTaskDelay(pdMS_TO_TICKS(420));            // lo que tarda la conversion de 11 bits
    float tubo = termTubo.getTempCByIndex(0);
    float agua = termAgua.getTempCByIndex(0);
    if (!termometroOk(tubo)) tubo = NAN;       // -127 = no contesta, 85 = lectura sin terminar
    if (!termometroOk(agua)) agua = NAN;

    Entradas e;
    portENTER_CRITICAL(&mux);
    e = gOrdenes;
    portEXIT_CRITICAL(&mux);
    e.tCamara = cam; e.tTubo = tubo; e.tAgua = agua;

    Salidas s = decidir(e, ctl, millis());

    digitalWrite(PIN_BOMBA,   s.bomba   ? HIGH : LOW);
    digitalWrite(PIN_CELDA_A, s.celdaA  ? HIGH : LOW);
    digitalWrite(PIN_CELDA_B, s.celdaB  ? HIGH : LOW);

    portENTER_CRITICAL(&mux);
    gSalidas = s;
    gCam = cam; gTubo = tubo; gAgua = agua; gHum = hum;
    gMsOnA = ctl.msOnA; gMsOnB = ctl.msOnB;
    gRocio = puntoDeRocio(cam, hum);
    portEXIT_CRITICAL(&mux);

    uint32_t usado = millis() - t0;
    vTaskDelay(pdMS_TO_TICKS(usado < 2000 ? 2000 - usado : 20));
  }
}

/* ---------------------------------------------------------------------------
 * NUBE: lee las ordenes de la app y publica lo que esta pasando
 * ------------------------------------------------------------------------- */
static float redondear(float v) { return isnan(v) ? 0.0f : roundf(v * 10.0f) / 10.0f; }

/* Solo publica la lectura si existe: un sensor ausente no debe aparecer como 0 grados. */
static void ponerLectura(FirebaseJson &j, const char *clave, float v) {
  if (!isnan(v)) j.set(clave, roundf(v * 10.0f) / 10.0f);
}

void iniciarFirebase() {
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  Firebase.setReadTimeout(fbdo, 3000);   // que una nube lenta no sea un problema
  firebaseIniciado = true;
  Serial.println("Firebase iniciado.");
}

/* Una sola consulta trae setpoint, modo y los interruptores de la app. */
/* ---------------------------------------------------------------------------
 * COMANDOS POR USB (Monitor Serie a 115200, terminar con Enter):
 *   obj 23        objetivo de la camara (15 a 40 C)
 *   modo auto     modo auto | manual | off
 *   a 1  /  b 0   celda A o B en modo manual
 *   carga 1       aviso de carga alta (1 o 0)
 * Se aplican al instante y se guardan en Firebase (config), igual que desde la app.
 * ------------------------------------------------------------------------- */
void atenderComandosSerie() {
  static String linea;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch != '\n' && ch != '\r') { if (linea.length() < 40) linea += ch; continue; }
    linea.trim();
    if (linea.length() == 0) continue;
    String cmd = linea; linea = "";
    cmd.toLowerCase();
    int esp = cmd.indexOf(' ');
    String clave = esp < 0 ? cmd : cmd.substring(0, esp);
    String valor = esp < 0 ? "" : cmd.substring(esp + 1);
    valor.trim();

    portENTER_CRITICAL(&mux);
    Entradas n = gOrdenes;
    portEXIT_CRITICAL(&mux);
    bool ok = true;
    if (clave == "obj") {
      float v = valor.toFloat();
      if (v >= 15.0f && v <= 40.0f) { n.setpoint = v; pendienteConfig.set("setpoint", v); }
      else ok = false;
    } else if (clave == "modo") {
      if (valor == "auto" || valor == "manual" || valor == "off") {
        n.modo = valor == "manual" ? MODO_MANUAL : valor == "off" ? MODO_APAGADO : MODO_AUTO;
        pendienteConfig.set("modo", valor);
      } else ok = false;
    } else if (clave == "a" || clave == "b") {
      bool v = valor == "1";
      if (clave == "a") { n.manualA = v; pendienteConfig.set("manual_a", v); }
      else              { n.manualB = v; pendienteConfig.set("manual_b", v); }
    } else if (clave == "carga") {
      n.cargaAlta = valor == "1";
      pendienteConfig.set("carga_alta", n.cargaAlta);
    } else {
      ok = false;
    }
    if (!ok) { Serial.printf(">>> Comando no valido: %s\n", cmd.c_str()); continue; }
    portENTER_CRITICAL(&mux);
    gOrdenes.setpoint = n.setpoint; gOrdenes.modo = n.modo;
    gOrdenes.manualA = n.manualA;   gOrdenes.manualB = n.manualB;
    gOrdenes.cargaAlta = n.cargaAlta;
    portEXIT_CRITICAL(&mux);
    hayPendienteConfig = true;
    Serial.printf(">>> OK: %s\n", cmd.c_str());
  }
}

void leerOrdenes() {
  if (!Firebase.getJSON(fbdo, RUTA_CONFIG)) return;

  if (fbdo.dataType() == "null") {                 // primera vez: crear valores iniciales
    FirebaseJson ini;
    ini.set("setpoint", 28.0);
    ini.set("modo", "auto");
    ini.set("manual_a", false);
    ini.set("manual_b", false);
    ini.set("carga_alta", false);
    Firebase.updateNode(fbdo, RUTA_CONFIG, ini);
    return;
  }

  FirebaseJson *j = fbdo.jsonObjectPtr();
  FirebaseJsonData r;
  Entradas n;
  portENTER_CRITICAL(&mux);
  n = gOrdenes;
  portEXIT_CRITICAL(&mux);

  if (j->get(r, "setpoint") && r.success) {
    float v = r.to<float>();
    if (v >= 15.0f && v <= 40.0f) n.setpoint = v;  // un valor absurdo no se obedece
  }
  if (j->get(r, "modo") && r.success) {
    String m = r.to<String>();
    n.modo = (m == "manual") ? MODO_MANUAL : (m == "off" || m == "apagado") ? MODO_APAGADO : MODO_AUTO;
  }
  if (j->get(r, "manual_a") && r.success)   n.manualA = r.to<bool>();
  if (j->get(r, "manual_b") && r.success)   n.manualB = r.to<bool>();
  if (j->get(r, "carga_alta") && r.success) n.cargaAlta = r.to<bool>();

  portENTER_CRITICAL(&mux);
  gOrdenes.setpoint = n.setpoint; gOrdenes.modo = n.modo;
  gOrdenes.manualA = n.manualA;   gOrdenes.manualB = n.manualB;
  gOrdenes.cargaAlta = n.cargaAlta;
  portEXIT_CRITICAL(&mux);
}

void publicar() {
  Salidas s; Entradas o; float cam, tubo, agua, hum, rocio;
  portENTER_CRITICAL(&mux);
  s = gSalidas; o = gOrdenes;
  cam = gCam; tubo = gTubo; agua = gAgua; hum = gHum; rocio = gRocio;
  portEXIT_CRITICAL(&mux);

  FirebaseJson l;
  ponerLectura(l, "ambiente",    cam);
  ponerLectura(l, "humedad",     hum);
  ponerLectura(l, "punto_rocio", rocio);
  ponerLectura(l, "placa_fria",  tubo);
  ponerLectura(l, "disipador_a", agua);
  ponerLectura(l, "aire_salida", cam);
  ponerLectura(l, "delta_t",     (isnan(cam) || isnan(tubo)) ? NAN : cam - tubo);
  ponerLectura(l, "pendiente",   s.pendiente);
  ponerLectura(l, "prediccion",  s.prediccion);
  Firebase.updateNode(fbdo, RUTA_LECTURAS, l);
#if SENSOR_AGUA == 0
  static bool borrado = false;          // quita una vez la lectura vieja del agua
  if (!borrado) borrado = Firebase.deleteNode(fbdo, RUTA_LECTURAS "/disipador_a");
#endif

  FirebaseJson st;
  st.set("nivel",      (int)s.nivel);
  st.set("celda_a",    s.celdaA);
  st.set("celda_b",    s.celdaB);
  st.set("bloqueo_a",  s.congelado || s.sinHielo);
  st.set("bloqueo_b",  s.congelado || s.sinHielo);
  st.set("bomba",      s.bomba);
  st.set("pwm_fans",   s.bomba ? 100 : 0);
  st.set("setpoint",   o.setpoint);
  st.set("modo",       o.modo == MODO_MANUAL ? "manual" : o.modo == MODO_APAGADO ? "off" : "auto");
  st.set("motivo",     MOTIVOS[s.motivo]);
  st.set("alerta",     ALERTAS[s.alerta]);
  st.set("condensa",   (!isnan(tubo) && !isnan(rocio)) ? (tubo < rocio) : false);
  st.set("carga_alta", o.cargaAlta);
  st.set("online",     true);
  st.set("uptime_s",   (int)(millis() / 1000));
  Firebase.updateNode(fbdo, RUTA_ESTADO, st);
}

/* ---------------------------------------------------------------------------
 * PANTALLA: 4 vistas que rotan cada 3 s; si hay alerta, se intercala.
 * ------------------------------------------------------------------------- */
void pantalla(const Salidas &s, float tubo, float agua, float hum, float rocio,
              uint32_t msA, uint32_t msB) {
  static uint8_t paso = 0;
  char a[24], b[24];
  uint8_t vista = paso % 3;
  if (s.alerta != A_NINGUNA && (paso % 2) == 0) vista = 9;    // la alerta se intercala
  paso++;

  switch (vista) {
    case 0:                                       // el modulo
      {
        char t1[8], t2[8];
        if (isnan(tubo)) snprintf(t1, sizeof(t1), "   --"); else snprintf(t1, sizeof(t1), "%5.1f", tubo);
        if (isnan(agua)) snprintf(t2, sizeof(t2), "   --"); else snprintf(t2, sizeof(t2), "%5.1f", agua);
        snprintf(a, sizeof(a), "Blq%s Ag%s", t1, t2);
      }
      snprintf(b, sizeof(b), "Roc%5.1f Hum%3.0f%%", rocio, hum);
      break;
    case 1:                                       // el uso de las celdas
      snprintf(a, sizeof(a), "Uso celdas (h)");
      snprintf(b, sizeof(b), "A%6.2f B%6.2f", msA / 3600000.0f, msB / 3600000.0f);
      break;
    case 2:                                       // la red
      if (WiFi.status() == WL_CONNECTED) {
        snprintf(a, sizeof(a), "WiFi%4ddBm %s", (int)WiFi.RSSI(),
                 (firebaseIniciado && Firebase.ready()) ? "FB" : "--");
        snprintf(b, sizeof(b), "%s", WiFi.localIP().toString().c_str());
      } else {
        snprintf(a, sizeof(a), "Sin WiFi. Red:");
        snprintf(b, sizeof(b), "ESP32_Emilio");
      }
      break;
    default:
      snprintf(a, sizeof(a), "** ALERTA **");
      if (s.alerta == A_SENSOR)
        snprintf(b, sizeof(b), "Falla:%s%s%s", (s.fallas & 2) ? " BLQ" : "",
                 (s.fallas & 4) ? " AGU" : "", (s.fallas & 1) ? " CAM" : "");
      else
        snprintf(b, sizeof(b), "%.16s", s.alerta == A_CONGELA ? "Bloque congelado" : "Sin hielo/bomba");
      break;
  }
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(a);
  lcd.setCursor(0, 1); lcd.print(b);
}

/* ---------------------------------------------------------------------------
 * OLED 0.96": lo que decide el control. Va en su propio bus I2C (Wire1, GPIO 16 y 17).
 *
 *   N2 28.3 C     A B ~      <- nivel y camara grandes; A, B y bomba en negativo si encienden
 *                 ^ obj28    <- tendencia (sube, baja, estable) y objetivo
 *   [grafica de la camara, ultimos ~4 min, con el objetivo punteado]
 *   Sobre objetivo           <- motivo de la decision (parpadea si hay alerta)
 *
 * Si no esta conectada, el resto del programa sigue igual.
 * ------------------------------------------------------------------------- */
#define HIST_N 128                     // una muestra cada 2 s = 4 min 16 s en pantalla
float histCam[HIST_N];
uint8_t histTotal = 0, histSig = 0;

void iniciarOled() {
  Wire1.begin(OLED_SDA, OLED_SCL, 400000);   // la OLED aguanta 400 kHz: dibuja en ~25 ms
  const uint8_t direcciones[] = { 0x3C, 0x3D };   // 0x78 y 0x7A en la placa
  for (uint8_t dir : direcciones) {
    Wire1.beginTransmission(dir);
    if (Wire1.endTransmission() == 0 && oled.begin(SSD1306_SWITCHCAPVCC, dir, true, false)) {
      hayOled = true;
      Serial.printf("OLED detectada en 0x%02X\n", dir);
      break;
    }
  }
  if (!hayOled) {
    Serial.println("[!] No respondio la OLED. El sistema sigue con la LCD.");
    return;
  }
  oled.cp437(true);                    // para usar el signo de grado y las flechas
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2); oled.setCursor(22, 18); oled.print("FROSTER");
  oled.setTextSize(1); oled.setCursor(28, 44); oled.print("iniciando...");
  oled.display();
}

void registrarHistorial(float cam) {
  static uint32_t t = 0;
  if (isnan(cam) || millis() - t < 2000UL) return;
  t = millis();
  histCam[histSig] = cam;
  histSig = (histSig + 1) % HIST_N;
  if (histTotal < HIST_N) histTotal++;
}

static void indicador(int16_t x, char letra, bool encendido) {
  if (encendido) {
    oled.fillRect(x - 1, 0, 8, 8, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
  } else {
    oled.setTextColor(SSD1306_WHITE);
  }
  oled.setCursor(x, 0);
  oled.write(letra);
  oled.setTextColor(SSD1306_WHITE);
}

void pantallaOled(const Salidas &s, const Entradas &o, float cam) {
  if (!hayOled) return;
  char b[32];
  const uint8_t nivel = s.nivel ? s.nivel : 1;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // ---- encabezado
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  if (isnan(cam)) snprintf(b, sizeof(b), "N%u --.-", nivel);
  else            snprintf(b, sizeof(b), "N%u %4.1f", nivel, cam);
  oled.print(b);
  oled.setTextSize(1);
  oled.setCursor(86, 0); oled.write(0xF8); oled.print("C");
  indicador(100, 'A', s.celdaA);
  indicador(110, 'B', s.celdaB);
  indicador(120, '~', s.bomba);
  oled.setCursor(86, 8);
  oled.write(s.pendiente > 0.15f ? 0x18 : (s.pendiente < -0.15f ? 0x19 : '='));
  snprintf(b, sizeof(b), "obj%2.0f", o.setpoint);
  oled.setCursor(98, 8); oled.print(b);

  // ---- grafica de la camara
  const int16_t G0 = 18, GH = 36;
  if (histTotal >= 2) {
    float mn = o.setpoint, mx = o.setpoint;
    for (uint8_t i = 0; i < histTotal; i++) {
      float v = histCam[(histSig + HIST_N - histTotal + i) % HIST_N];
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
    if (mx - mn < 2.0f) { float c = (mx + mn) / 2; mn = c - 1.0f; mx = c + 1.0f; }
    auto Y = [&](float v) -> int16_t {
      return G0 + GH - 1 - (int16_t)lroundf((v - mn) / (mx - mn) * (GH - 1));
    };
    int16_t yObj = Y(o.setpoint);
    for (int16_t x = 0; x < 128; x += 4) oled.drawFastHLine(x, yObj, 2, SSD1306_WHITE);
    int16_t x0 = 128 - histTotal;
    float previo = histCam[(histSig + HIST_N - histTotal) % HIST_N];
    for (uint8_t i = 1; i < histTotal; i++) {
      float v = histCam[(histSig + HIST_N - histTotal + i) % HIST_N];
      oled.drawLine(x0 + i - 1, Y(previo), x0 + i, Y(v), SSD1306_WHITE);
      previo = v;
    }
    oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    snprintf(b, sizeof(b), "%.1f", mx); oled.setCursor(0, G0);          oled.print(b);
    snprintf(b, sizeof(b), "%.1f", mn); oled.setCursor(0, G0 + GH - 8); oled.print(b);
    oled.setTextColor(SSD1306_WHITE);
  } else {
    oled.setCursor(10, 32); oled.print("juntando datos...");
  }

  // ---- renglon de abajo: motivo de la decision, o la alerta parpadeando
  if (s.alerta != A_NINGUNA) {
    if ((millis() / 1000) % 2) {
      oled.fillRect(0, 56, 128, 8, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
    }
    snprintf(b, sizeof(b), "! %s", MOTIVOS[s.motivo]);
    oled.setCursor(2, 56); oled.print(b);
    oled.setTextColor(SSD1306_WHITE);
  } else {
    oled.setCursor(0, 56); oled.print(MOTIVOS[s.motivo]);
  }
  oled.display();
}

/* ---------------------------------------------------------------------------
 * SENALES: LED RGB y buzzer pasivo
 * El LED muestra el estado todo el tiempo. El buzzer suena solo en los cambios:
 * una vez por cambio de nivel, y las alertas como maximo una vez por minuto.
 * Las melodias se tocan sin bloquear: el loop avanza una nota cuando toca.
 * ------------------------------------------------------------------------- */
const Nota MEL_ARRANQUE[]  = { {784,120},{0,40},{988,120},{0,40},{1319,220},{0,0} };
const Nota MEL_SUBE_2[]    = { {880,120},{0,40},{1175,160},{0,0} };
const Nota MEL_SUBE_3[]    = { {880,110},{0,30},{1175,110},{0,30},{1568,180},{0,0} };
const Nota MEL_BAJA[]      = { {1175,140},{0,40},{784,220},{0,0} };
const Nota MEL_CONGELA[]   = { {2093,90},{0,90},{2093,90},{0,90},{2093,90},{0,0} };
const Nota MEL_SIN_HIELO[] = { {1047,400},{0,150},{1047,400},{0,0} };
const Nota MEL_SENSOR[]    = { {330,250},{0,150},{330,250},{0,150},{330,250},{0,0} };

const Nota *melodia = nullptr;
uint8_t notaIdx = 0;
uint32_t tNota = 0;

void tocar(const Nota *m) { melodia = m; notaIdx = 0; tNota = 0; }

void silencio() {
#if BUZZER_ACTIVO_BAJO
  ledcWrite(PIN_BUZZER, (1 << 10) - 1);     // 100 %: salida en ALTO = modulo sin corriente
#else
  ledcWriteTone(PIN_BUZZER, 0);             // salida en BAJO
#endif
}

void atenderBuzzer() {
  if (!melodia) return;
  uint32_t ahora = millis();
  if (tNota != 0) {
    if (ahora - tNota < melodia[notaIdx].ms) return;   // la nota sigue sonando
    notaIdx++;
  }
  const Nota &n = melodia[notaIdx];
  if (n.ms == 0) { silencio(); melodia = nullptr; return; }
  if (n.hz) ledcWriteTone(PIN_BUZZER, n.hz); else silencio();
  tNota = ahora ? ahora : 1;
}

void color(uint8_t r, uint8_t g, uint8_t b) {
#if LED_ANODO_COMUN
  r = 255 - r; g = 255 - g; b = 255 - b;    // en anodo comun, ALTO = apagado
#endif
  ledcWrite(PIN_LED_R, r);
  ledcWrite(PIN_LED_G, g);
  ledcWrite(PIN_LED_B, b);
}

void iniciarSenales() {
  ledcAttach(PIN_LED_R, 5000, 8);
  ledcAttach(PIN_LED_G, 5000, 8);
  ledcAttach(PIN_LED_B, 5000, 8);
  ledcAttach(PIN_BUZZER, 2000, 10);
  silencio();
  color(0, 0, 255);                         // azul: arrancando
}

void gestionarSenales(const Salidas &s) {
  static bool saludo = false;
  static uint8_t nivelPrev = 1, alertaPrev = A_NINGUNA;
  static uint32_t tAlerta = 0;
  const uint32_t ahora = millis();
  const bool encendido = (ahora / 400) % 2;  // parpadeo de 0.4 s
  const uint8_t nivel = s.nivel ? s.nivel : 1;  // 0 = el control aun no decide

  // ---- LED: un color por estado
  if (ahora < 10000UL)               encendido ? color(0, 0, 255)   : color(0, 0, 0);
  else if (s.alerta == A_CONGELA)    encendido ? color(160, 0, 255) : color(0, 0, 0);
  else if (s.alerta == A_SIN_HIELO)  encendido ? color(255, 80, 0)  : color(0, 0, 0);
  else if (s.alerta == A_SENSOR)     encendido ? color(255, 0, 0)   : color(0, 0, 0);
  else if (s.motivo == M_PRECIRC || s.motivo == M_ESPERA) color(255, 180, 0);
  else if (nivel == 3)               color(0, 0, 255);
  else if (nivel == 2)               color(0, 200, 200);
  else                               color(0, 255, 0);

  // ---- Buzzer: un sonido por evento
  if (!saludo) {
    if (ahora > 1500UL) { saludo = true; tocar(MEL_ARRANQUE); }
  } else if (s.alerta != A_NINGUNA) {
    if (s.alerta != alertaPrev || ahora - tAlerta >= 60000UL) {
      tAlerta = ahora;
      tocar(s.alerta == A_CONGELA ? MEL_CONGELA : s.alerta == A_SIN_HIELO ? MEL_SIN_HIELO : MEL_SENSOR);
    }
  } else if (nivel != nivelPrev) {
    tocar(nivel > nivelPrev ? (nivel == 3 ? MEL_SUBE_3 : MEL_SUBE_2) : MEL_BAJA);
  }
  alertaPrev = s.alerta;
  nivelPrev = nivel;
  atenderBuzzer();
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
  if (ahora - tOled >= 1000UL && melodia == nullptr) {   // no interrumpir una melodia:
    tOled = ahora;                                        // dibujar tarda ~0.1 s
    pantallaOled(s, o, cam);
  }
  gestionarSenales(s);
  delay(10);                                      // 10 ms: notas del buzzer a tiempo
}
