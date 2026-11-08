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
  const float pct = (FUGA_SECO_RAW - analogRead(PIN_FUGA)) * 100.0f
                    / (float)(FUGA_SECO_RAW - FUGA_MOJADO_RAW);
  return constrain(pct, 0.0f, 100.0f);
#else
  return NAN;
#endif
}

void tareaControl(void *arg) {
  /* Los DS18B20 tardan ~400 ms en convertir. En vez de esperarlos, se lee la
     conversion que se pidio en la vuelta anterior y se pide la siguiente. */
  termTubo.requestTemperatures();
  termAgua.requestTemperatures();
  TickType_t ultimo = xTaskGetTickCount();
  FiltroFuga filtroFuga = {};
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
    const float fuga = leerFuga();
    e.fuga = filtrarFuga(filtroFuga, fuga, millis());

    Salidas s = decidir(e, ctl, millis());

    ledcWrite(PIN_BOMBA, cicloVentilacion(s));
    digitalWrite(PIN_CELDA_A, s.celdaA  ? HIGH : LOW);
    digitalWrite(PIN_CELDA_B, s.celdaB  ? HIGH : LOW);

    portENTER_CRITICAL(&mux);
    gSalidas = s;
    gCam = cam; gTubo = tubo; gAgua = agua; gHum = hum; gFugaPct = fuga;
    gMsOnA = ctl.msOnA; gMsOnB = ctl.msOnB;
    gRocio = puntoDeRocio(cam, hum);
    portEXIT_CRITICAL(&mux);

    /* Cede el procesador hasta la siguiente vuelta: no es un delay(), el loop()
       y la red siguen trabajando mientras tanto. */
    vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(2000));
  }
}
