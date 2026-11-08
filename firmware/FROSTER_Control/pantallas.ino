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
        if (isnan(tubo)) snprintf(t1, sizeof(t1), "  --"); else snprintf(t1, sizeof(t1), "%4.1f", tubo);
        // fila de arriba: bloque frio y humedad del sensor de fuga
        if (isnan(gFugaPct)) snprintf(t2, sizeof(t2), "  --"); else snprintf(t2, sizeof(t2), "%3.0f%%", gFugaPct);
        snprintf(a, sizeof(a), "Blq%s Fuga%s", t1, t2);     // "Blq22.5 Fuga 12%": 16 letras
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
