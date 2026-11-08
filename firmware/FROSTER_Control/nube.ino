/* ---------------------------------------------------------------------------
 * NUBE: WiFi, Firebase Realtime Database y ordenes por USB
 *
 * Arbol en Firebase (todo bajo RUTA_BASE = froster):
 *   lecturas/     sensores, numeros con 1 decimal + ts (hora NTP en ms)
 *   estado/       lo que hace el sistema: nivel, salidas, modo, motivo, alerta
 *   config/       lo que piden la app y el tablero (el ESP32 solo lo lee)
 *   ack/          confirmacion de cada comando que trae cmd_id (para medir latencia)
 *   dispositivo/  red y conexion: ip, rssi, caidas, firmware, ultima conexion
 *   eventos/      bitacora: cambios de nivel, alertas y picos de temperatura
 *
 * Trafico: los comandos llegan por un stream (al instante, sin preguntar cada
 * pocos segundos) y las lecturas y el estado solo se suben cuando cambian, mas
 * un latido cada 10 s para que la app sepa que la placa sigue viva.
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
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // hora real para las marcas de tiempo
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

/* Hora real en milisegundos desde 1970 (NTP). 0 si todavia no se sincroniza. */
static double horaMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < 1700000000L) return 0;
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000;
}

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
  fbStream.setBSSLBufferSize(2048, 512);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  Firebase.setReadTimeout(fbdo, 3000);   // que una nube lenta no sea un problema
  firebaseIniciado = true;
  Serial.println("Firebase iniciado.");
}

/* ---------------------------------------------------------------------------
 * COMANDOS DESDE LA APP Y EL TABLERO (stream sobre config/)
 * El callback corre en la tarea de la libreria: solo valida, guarda las ordenes
 * y avisa al loop. Las escrituras a Firebase (ack, valores iniciales) las hace
 * el loop, nunca el callback.
 * ------------------------------------------------------------------------- */
static volatile bool crearConfigInicial = false;
static volatile double ackPendiente = 0;       // cmd_id por confirmar (0 = nada)
static volatile uint32_t tUltimoComando = 0;   // millis() al recibirlo, para medir el atraso local
static double idUsb = 0;                       // cmd_id del ultimo comando escrito por USB
static uint32_t tEnvioUsb = 0;                 // millis() al subirlo: mide la ida y vuelta por Firebase

static float numeroDe(StreamData &d) {
  switch (d.dataTypeEnum()) {
    case firebase_rtdb_data_type_integer: return (float)d.intData();
    case firebase_rtdb_data_type_float:   return d.floatData();
    case firebase_rtdb_data_type_double:  return (float)d.doubleData();
    default:                              return NAN;
  }
}

/* Aplica una clave de config/ sobre las ordenes. Un valor absurdo no se obedece. */
static void aplicarOrden(Entradas &n, const String &clave, float num, bool bit, const String &texto) {
  if (clave == "setpoint" && !isnan(num) && num >= 15.0f && num <= 40.0f) n.setpoint = num;
  else if (clave == "modo") n.modo = (texto == "manual") ? MODO_MANUAL
                                   : (texto == "off" || texto == "apagado") ? MODO_APAGADO : MODO_AUTO;
  else if (clave == "manual_a")   n.manualA = bit;
  else if (clave == "manual_b")   n.manualB = bit;
  else if (clave == "carga_alta") n.cargaAlta = bit;
}

static void guardarOrdenes(const Entradas &n) {
  portENTER_CRITICAL(&mux);
  gOrdenes.setpoint = n.setpoint; gOrdenes.modo = n.modo;
  gOrdenes.manualA = n.manualA;   gOrdenes.manualB = n.manualB;
  gOrdenes.cargaAlta = n.cargaAlta;
  portEXIT_CRITICAL(&mux);
}

void alRecibirConfig(StreamData datos) {
  portENTER_CRITICAL(&mux);
  Entradas n = gOrdenes;
  portEXIT_CRITICAL(&mux);

  const String ruta = datos.dataPath();          // "/" = objeto completo o parcial; "/clave" = un valor
  double cmd = 0;
  if (datos.dataTypeEnum() == firebase_rtdb_data_type_null && ruta == "/") {
    crearConfigInicial = true;                   // config/ no existe todavia
    return;
  }
  if (datos.dataTypeEnum() == firebase_rtdb_data_type_json) {
    FirebaseJson &j = datos.jsonObject();
    FirebaseJsonData r;
    const char *claves[] = { "setpoint", "modo", "manual_a", "manual_b", "carga_alta" };
    for (const char *c : claves) {
      if (!j.get(r, c) || !r.success) continue;
      aplicarOrden(n, c, r.type == "string" ? NAN : r.to<float>(), r.to<bool>(), r.to<String>());
    }
    if (j.get(r, "cmd_id") && r.success) cmd = r.to<double>();
  } else {
    const String clave = ruta.substring(1);
    if (clave == "cmd_id") cmd = numeroDe(datos) > 0 ? datos.doubleData() : 0;
    else aplicarOrden(n, clave, numeroDe(datos), datos.boolData(), datos.stringData());
  }
  guardarOrdenes(n);
  if (cmd > 0) { tUltimoComando = millis(); ackPendiente = cmd; }
}

void alCortarseStream(bool vencido) {
  if (vencido) Serial.println("Stream de config: se reanuda solo");
}

static void iniciarStream() {
  static bool iniciado = false;
  static uint32_t tIntento = 0;
  if (iniciado || millis() - tIntento < 5000UL) return;
  tIntento = millis();
  fbStream.keepAlive(5, 5, 1);
  if (!Firebase.beginStream(fbStream, RUTA_CONFIG)) {
    Serial.printf("Stream de config no inicio: %s\n", fbStream.errorReason().c_str());
    return;                                      // se reintenta en 5 s
  }
  Firebase.setStreamCallback(fbStream, alRecibirConfig, alCortarseStream);
  iniciado = true;
  Serial.println("Escuchando config/ en tiempo real.");
}

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
    guardarOrdenes(n);
    hayPendienteConfig = true;
    Serial.printf(">>> OK: %s\n", cmd.c_str());
  }
}

/* ---------------------------------------------------------------------------
 * PUBLICACION CON FILTRO
 * ------------------------------------------------------------------------- */
#define LATIDO_MS           10000UL   // cada cuanto se avisa que la placa sigue viva
#define DISPOSITIVO_MS      30000UL   // datos de red: cambian poco
#define PICO_MARGEN_C       2.5f      // camara por arriba del objetivo que cuenta como pico
#define PICO_HISTERESIS_C   0.5f

/* Cuanto tiene que cambiar cada lectura para volver a subirla. Mas chico que el
   ruido del sensor solo gastaria datos sin dar informacion nueva. */
struct Lectura { const char *clave; float umbral; float enviado; };
static Lectura lecturas[] = {
  { "ambiente", 0.2f, NAN }, { "humedad", 1.0f, NAN }, { "punto_rocio", 0.3f, NAN },
  { "placa_fria", 0.2f, NAN }, { "fuga_pct", 2.0f, NAN }, { "pendiente", 0.05f, NAN },
  { "prediccion", 0.2f, NAN },
};
static const size_t N_LECTURAS = sizeof(lecturas) / sizeof(lecturas[0]);

/* Estadistica de lo que tarda cada envio a Firebase, para el Monitor Serie. */
static uint32_t envios = 0, sumaMs = 0, maxMs = 0;

static bool subir(const char *ruta, FirebaseJson &j) {
  const uint32_t t0 = millis();
  const bool ok = Firebase.updateNode(fbdo, ruta, j);
  const uint32_t ms = millis() - t0;
  if (ok) { envios++; sumaMs += ms; if (ms > maxMs) maxMs = ms; }
  else Serial.printf("Firebase: no se pudo escribir %s (%s)\n", ruta, fbdo.errorReason().c_str());
  return ok;
}

static void evento(const char *tipo, const String &texto, uint8_t nivel, float cam, float objetivo) {
  FirebaseJson j;
  j.set("tipo", tipo);
  j.set("texto", texto);
  j.set("nivel", (int)nivel);
  ponerLectura(j, "camara", cam);
  j.set("setpoint", objetivo);
  j.set("ts/.sv", "timestamp");                  // hora del servidor de Firebase
  if (!Firebase.pushJSON(fbdo, RUTA_EVENTOS, j))
    Serial.printf("Firebase: no se pudo guardar el evento (%s)\n", fbdo.errorReason().c_str());
}

static void publicar() {
  Salidas s; Entradas o; float cam, tubo, hum, rocio, fuga;
  portENTER_CRITICAL(&mux);
  s = gSalidas; o = gOrdenes;
  cam = gCam; tubo = gTubo; hum = gHum; rocio = gRocio; fuga = gFugaPct;
  portEXIT_CRITICAL(&mux);
  const uint32_t ahora = millis();

  // ---- lecturas: solo si alguna cambio de verdad, o en el latido
  static uint32_t tLecturas = 0;
  const float valores[N_LECTURAS] = { cam, hum, rocio, tubo, fuga, s.pendiente, s.prediccion };
  bool cambio = tLecturas == 0 || (ahora - tLecturas) >= LATIDO_MS;
  for (size_t i = 0; i < N_LECTURAS; i++) {
    const float v = valores[i], e = lecturas[i].enviado;
    if (isnan(v) != isnan(e) || (!isnan(v) && fabsf(v - e) >= lecturas[i].umbral)) cambio = true;
  }
  if (cambio) {
    FirebaseJson l;
    for (size_t i = 0; i < N_LECTURAS; i++) ponerLectura(l, lecturas[i].clave, valores[i]);
    const double ts = horaMs();
    if (ts > 0) l.set("ts", ts);
    if (subir(RUTA_LECTURAS, l)) {
      for (size_t i = 0; i < N_LECTURAS; i++) lecturas[i].enviado = valores[i];
      tLecturas = ahora;
    }
  }

  // ---- estado: completo cuando cambia; en el latido, solo uptime_s
  FirebaseJson st;
  st.set("nivel",      (int)s.nivel);
  st.set("celda_a",    s.celdaA);
  st.set("celda_b",    s.celdaB);
  st.set("bomba",      s.bomba);
  st.set("pwm_fans",   (int)lroundf(cicloVentilacion(s) * 100.0f / 255.0f));
  st.set("setpoint",   o.setpoint);
  st.set("modo",       o.modo == MODO_MANUAL ? "manual" : o.modo == MODO_APAGADO ? "off" : "auto");
  st.set("motivo",     MOTIVOS[s.motivo]);
  st.set("alerta",     ALERTAS[s.alerta]);
  st.set("alerta_id",  (int)s.alerta);
  st.set("condensa",   (!isnan(tubo) && !isnan(rocio)) ? (tubo < rocio) : false);
  st.set("carga_alta", o.cargaAlta);
  String firma;
  st.toString(firma);
  static String firmaEnviada;
  static uint32_t tLatido = 0;
  if (firma != firmaEnviada) {
    st.set("uptime_s", (int)(ahora / 1000));
    if (subir(RUTA_ESTADO, st)) { firmaEnviada = firma; tLatido = ahora; }
  } else if (ahora - tLatido >= LATIDO_MS) {
    FirebaseJson latido;
    latido.set("uptime_s", (int)(ahora / 1000));
    if (subir(RUTA_ESTADO, latido)) tLatido = ahora;
  }

  // ---- dispositivo: red y conexion
  static uint32_t tDisp = 0, caidasEnviadas = UINT32_MAX;
  if (tDisp == 0 || ahora - tDisp >= DISPOSITIVO_MS || gCaidasWiFi != caidasEnviadas) {
    FirebaseJson d;
    d.set("ip", WiFi.localIP().toString());
    d.set("rssi", (int)WiFi.RSSI());
    d.set("caidas_wifi", (int)gCaidasWiFi);
    d.set("firmware", FIRMWARE_VERSION);
    d.set("ultima_conexion/.sv", "timestamp");
    if (subir(RUTA_DISPOSITIVO, d)) { tDisp = ahora; caidasEnviadas = gCaidasWiFi; }
  }

  // ---- eventos: solo en las transiciones
  static uint8_t nivelPrev = 0, alertaPrev = A_NINGUNA;
  static bool enPico = false;
  if (s.nivel && nivelPrev && s.nivel != nivelPrev)
    evento("nivel", String("Nivel ") + s.nivel + ": " + MOTIVOS[s.motivo], s.nivel, cam, o.setpoint);
  if (s.alerta != alertaPrev)
    evento("alerta", s.alerta ? String(ALERTAS[s.alerta]) : String("Alerta resuelta"), s.nivel, cam, o.setpoint);
  if (!isnan(cam)) {
    if (!enPico && cam >= o.setpoint + PICO_MARGEN_C) {
      enPico = true;
      evento("pico", "Camara sobre el objetivo + 2.5 C", s.nivel, cam, o.setpoint);
    } else if (enPico && cam < o.setpoint + PICO_MARGEN_C - PICO_HISTERESIS_C) {
      enPico = false;
    }
  }
  if (s.nivel) nivelPrev = s.nivel;
  alertaPrev = s.alerta;
}

/* Todo lo de la nube, llamado en cada vuelta del loop. No bloquea mas que lo que
   tarda una escritura (unos cientos de ms) y solo cuando hay algo que enviar. */
void atenderNube() {
  if (!firebaseIniciado || WiFi.status() != WL_CONNECTED || !Firebase.ready()) return;
  iniciarStream();

  // 1. Confirmar el comando recibido: lo primero, porque de esto depende la latencia
  if (ackPendiente > 0) {
    const double id = ackPendiente;
    ackPendiente = 0;
    FirebaseJson a;
    a.set("cmd_id", id);
    const double ts = horaMs();
    if (ts > 0) a.set("recibido_ms", ts);
    const uint32_t local = millis() - tUltimoComando;
    a.set("aplicado_en_ms", (int)local);
    if (subir(RUTA_ACK, a)) Serial.printf(">>> Comando %.0f confirmado (%lu ms dentro del ESP32)\n", id, local);
    if (id == idUsb) {
      Serial.printf(">>> Ida y vuelta ESP32 -> Firebase -> ESP32: %lu ms\n", tUltimoComando - tEnvioUsb);
      idUsb = 0;
    }
  }

  // 2. config/ no existia: crear los valores de fabrica
  if (crearConfigInicial) {
    crearConfigInicial = false;
    FirebaseJson ini;
    ini.set("setpoint", 28.0);
    ini.set("modo", "auto");
    ini.set("manual_a", false);
    ini.set("manual_b", false);
    ini.set("carga_alta", false);
    subir(RUTA_CONFIG, ini);
  }

  // 3. Lo que se pidio por USB. Lleva cmd_id: al volver por el stream se mide la ida y vuelta.
  if (hayPendienteConfig) {
    const double id = horaMs() > 0 ? horaMs() : (double)millis();
    pendienteConfig.set("cmd_id", id);
    tEnvioUsb = millis();
    if (subir(RUTA_CONFIG, pendienteConfig)) {
      idUsb = id;
      pendienteConfig.clear();
      hayPendienteConfig = false;
    }
  }

  // 4. Lecturas, estado, dispositivo y eventos, revisados cada segundo
  static uint32_t tRevision = 0, tReporte = 0;
  if (millis() - tRevision >= 1000UL) {
    tRevision = millis();
    publicar();
  }

  // 5. Cada minuto, cuanto tardan las escrituras (evidencia de latencia)
  if (millis() - tReporte >= 60000UL) {
    tReporte = millis();
    if (envios) Serial.printf("Firebase: %lu envios en el ultimo minuto, promedio %lu ms, maximo %lu ms\n",
                              envios, sumaMs / envios, maxMs);
    envios = sumaMs = maxMs = 0;
  }
}
