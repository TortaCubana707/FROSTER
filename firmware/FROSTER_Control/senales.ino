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
const Nota MEL_FUGA[]      = { {1568,150},{0,60},{1047,150},{0,60},{1568,150},{0,60},{1047,150},{0,0} };

const Nota *melodia = nullptr;
uint8_t notaIdx = 0;
uint32_t tNota = 0;

void tocar(const Nota *m) { melodia = m; notaIdx = 0; tNota = 0; }

bool sonando() { return melodia != nullptr; }

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
  else if (s.alerta == A_FUGA)       encendido ? color(255, 255, 255) : color(0, 0, 0);
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
      tocar(s.alerta == A_CONGELA ? MEL_CONGELA : s.alerta == A_SIN_HIELO ? MEL_SIN_HIELO :
            s.alerta == A_FUGA ? MEL_FUGA : MEL_SENSOR);
    }
  } else if (nivel != nivelPrev) {
    tocar(nivel > nivelPrev ? (nivel == 3 ? MEL_SUBE_3 : MEL_SUBE_2) : MEL_BAJA);
  }
  alertaPrev = s.alerta;
  nivelPrev = nivel;
  atenderBuzzer();
}
