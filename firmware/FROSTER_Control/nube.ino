/* ---------------------------------------------------------------------------
 * NUBE: lee las ordenes de la app y publica lo que esta pasando
 * ------------------------------------------------------------------------- */

/* Reconexion autonoma: si se cae la red, la reintenta cada WIFI_REINTENTO_MS
   sin detener el control ni el loop. Si el portal de WiFiManager esta abierto
   (no hay red guardada), lo deja trabajar a el. */
void vigilarWiFi() {
  static bool conectado = false;
  static uint32_t tIntento = 0;
  if (WiFi.status() == WL_CONNECTED) {
    if (!conectado) {
      conectado = true;
      Serial.printf("WiFi conectado, IP %s, %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    return;
  }
  if (conectado) {
    conectado = false;
    gCaidasWiFi++;
    Serial.println("WiFi perdido: el control sigue; se reintenta cada 10 s");
  }
  if (wm.getConfigPortalActive()) return;
  if (millis() - tIntento >= WIFI_REINTENTO_MS) {
    tIntento = millis();
    Serial.println("Reintentando WiFi...");
    WiFi.reconnect();
  }
}
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
  ponerLectura(l, "fuga_pct",    gFugaPct);
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
  st.set("pwm_fans",   (int)lroundf(cicloVentilacion(s) * 100.0f / 255.0f));
  st.set("setpoint",   o.setpoint);
  st.set("modo",       o.modo == MODO_MANUAL ? "manual" : o.modo == MODO_APAGADO ? "off" : "auto");
  st.set("motivo",     MOTIVOS[s.motivo]);
  st.set("alerta",     ALERTAS[s.alerta]);
  st.set("condensa",   (!isnan(tubo) && !isnan(rocio)) ? (tubo < rocio) : false);
  st.set("carga_alta", o.cargaAlta);
  st.set("online",     true);
  st.set("uptime_s",   (int)(millis() / 1000));
  st.set("caidas_wifi", (int)gCaidasWiFi);
  Firebase.updateNode(fbdo, RUTA_ESTADO, st);
}
