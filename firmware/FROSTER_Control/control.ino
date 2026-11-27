/* ---------------------------------------------------------------------------
 * TAREA DE CONTROL
 * Lee, decide y mueve las salidas cada 2 s, pase lo que pase con el WiFi o con
 * Firebase. Si internet se cae o la nube tarda, esto no se entera.
 * ------------------------------------------------------------------------- */
/* Ciclo de trabajo de la ventilacion: apagada si no circula; si no, segun el nivel. */
uint8_t cicloVentilacion(const Salidas &s) {
  if (gPrueba == P_BOMBA && !gFuga) return 255;   // prueba: a tope unos segundos
  if (!s.bomba) return 0;
  if (!gPwmAuto) {
    /* Manual: lo que pida el tablero, pero con una celda encendida nunca menos del
       minimo, porque sin flujo de agua y de aire la celda se sobrecalienta. */
    uint8_t pct = gPwmPct;
    if ((s.celdaA || s.celdaB) && pct < PWM_MIN_CON_CELDA) pct = PWM_MIN_CON_CELDA;
    return (uint8_t)lroundf(pct * 255.0f / 100.0f);
  }
  if (s.nivel >= 3) return PWM_VENT_N3;
  if (s.nivel == 2) return PWM_VENT_N2;
  return PWM_VENT_N1;
}

static float calibrar(float v, float ajuste) { return isnan(v) ? v : v + ajuste; }

/* Humedad junto a la bomba: 0 % seco, 100 % mojado. NAN si no hay sensor. */
float leerFuga() {
#if SENSOR_FUGA
  gFugaRaw = analogRead(PIN_FUGA);
  const float pct = (FUGA_SECO_RAW - gFugaRaw) * 100.0f
                    / (float)(FUGA_SECO_RAW - FUGA_MOJADO_RAW);
  return constrain(pct, 0.0f, 100.0f);
#else
  return NAN;
#endif
}

/* Vigilancia rapida de la fuga, desde el loop cada 50 ms. Al confirmarla corta la bomba
   y las celdas en ese momento, sin esperar la siguiente vuelta del control (2 s); el
   control la ve en gFuga y mantiene todo apagado, con su alerta, LED y buzzer. */
void vigilarFuga() {
  static uint32_t tLectura = 0;
  static FiltroFuga filtro = {};
  const uint32_t ahora = millis();
  if (ahora - tLectura < 50UL) return;
  tLectura = ahora;
  const float pct = leerFuga();
  const bool fuga = filtrarFuga(filtro, pct, ahora);
  if (fuga && !gFuga) {
    ledcWrite(PIN_BOMBA, 0);
    digitalWrite(PIN_CELDA_A, LOW);
    digitalWrite(PIN_CELDA_B, LOW);
    Serial.printf(">>> FUGA: bomba y celdas cortadas (humedad %.0f %%)\n", pct);
  }
  portENTER_CRITICAL(&mux);
  gFuga = fuga;
  gFugaPct = pct;
  portEXIT_CRITICAL(&mux);
}

/* v2.1 - horas de uso en la memoria flash: se guardan cada 10 min y al reiniciar
   el contador sigue donde iba. */
#include <Preferences.h>
Preferences prefs;
void cargarHoras() {
  prefs.begin("froster", true);
  ctl.msOnA = prefs.getULong("msA", 0);
  ctl.msOnB = prefs.getULong("msB", 0);
  prefs.end();
}
static void guardarHoras(uint32_t a, uint32_t b) {
  prefs.begin("froster", false);
  prefs.putULong("msA", a);
  prefs.putULong("msB", b);
  prefs.end();
}

/* Pruebas de componentes pedidas desde el tablero. Solo enciende lo que es
   seguro encender: una celda necesita la bomba en marcha, el bloque arriba del
   corte por congelamiento y que no haya fuga. */
static void aplicarPrueba(Salidas &s, float tubo) {
  if (gPrueba == P_NINGUNA) return;
  if ((int32_t)(millis() - gPruebaHasta) >= 0) {
    if (gPruebaResultado.length() == 0) gPruebaResultado = String("Prueba de ") + PRUEBAS[gPrueba] + " terminada";
    gPrueba = P_NINGUNA;
    return;
  }
  if (gPrueba == P_CELDA_A || gPrueba == P_CELDA_B) {
    const bool segura = !gFuga && s.bomba && !isnan(tubo) && tubo > CONGELA_AVISO_C;
    if (!segura) {
      gPruebaResultado = "Prueba de celda cancelada: hace falta bomba encendida, bloque sobre "
                         + String(CONGELA_AVISO_C, 1) + " C y sin fuga";
      gPrueba = P_NINGUNA;
      return;
    }
    if (gPrueba == P_CELDA_A) s.celdaA = true; else s.celdaB = true;
  }
}

void tareaControl(void *arg) {
  /* Los DS18B20 tardan ~400 ms en convertir. En vez de esperarlos, se lee la
     conversion que se pidio en la vuelta anterior y se pide la siguiente. */
  termTubo.requestTemperatures();
  termAgua.requestTemperatures();
  TickType_t ultimo = xTaskGetTickCount();
  for (;;) {
    float cam = calibrar(dht.readTemperature(), CAL_CAMARA_C);
    float hum = calibrar(dht.readHumidity(), CAL_HUMEDAD);
    if (!isnan(hum)) hum = constrain(hum, 0.0f, 100.0f);
    float tubo = termTubo.getTempCByIndex(0);
    float agua = termAgua.getTempCByIndex(0);
    termTubo.requestTemperatures();            // queda lista para la siguiente vuelta
    termAgua.requestTemperatures();
    // -127 = no contesta, 85 = lectura sin terminar
    tubo = termometroOk(tubo) ? tubo + CAL_BLOQUE_C : NAN;
    if (!termometroOk(agua)) agua = NAN;

    Entradas e;
    portENTER_CRITICAL(&mux);
    e = gOrdenes;
    portEXIT_CRITICAL(&mux);
    e.tCamara = cam; e.tTubo = tubo; e.tAgua = agua;
    e.fuga = gFuga;

    if (gResetHoras) { gResetHoras = false; ctl.msOnA = ctl.msOnB = 0; guardarHoras(0, 0); }
    if (gCambioHz)   { gCambioHz = false; ledcChangeFrequency(PIN_BOMBA, PWM_VENT_HZ, 8); }
    Salidas s = decidir(e, ctl, millis());
    aplicarPrueba(s, tubo);
    static uint32_t tGuardado = 0;
    if (millis() - tGuardado >= 600000UL) { tGuardado = millis(); guardarHoras(ctl.msOnA, ctl.msOnB); }

    // Si la fuga se confirmo mientras se decidia, gana la fuga: nada se vuelve a encender
    const bool corte = gFuga;
    ledcWrite(PIN_BOMBA, corte ? 0 : cicloVentilacion(s));
    digitalWrite(PIN_CELDA_A, !corte && s.celdaA ? HIGH : LOW);
    digitalWrite(PIN_CELDA_B, !corte && s.celdaB ? HIGH : LOW);

    portENTER_CRITICAL(&mux);
    gSalidas = s;
    gCam = cam; gTubo = tubo; gAgua = agua; gHum = hum;
    gMsOnA = ctl.msOnA; gMsOnB = ctl.msOnB;
    gRocio = puntoDeRocio(cam, hum);
    portEXIT_CRITICAL(&mux);

    /* Cede el procesador hasta la siguiente vuelta: no es un delay(), el loop()
       y la red siguen trabajando mientras tanto. */
    vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(2000));
  }
}
