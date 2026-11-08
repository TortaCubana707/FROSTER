/* ---------------------------------------------------------------------------
 * TAREA DE CONTROL
 * Lee, decide y mueve las salidas cada 2 s, pase lo que pase con el WiFi o con
 * Firebase. Si internet se cae o la nube tarda, esto no se entera.
 * ------------------------------------------------------------------------- */
/* Ciclo de trabajo de la ventilacion: apagada si no circula; si no, segun el nivel. */
uint8_t cicloVentilacion(const Salidas &s) {
  if (!s.bomba) return 0;
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

    Salidas s = decidir(e, ctl, millis());

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
