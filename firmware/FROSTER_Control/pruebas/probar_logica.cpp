// Prueba de la logica de FROSTER_Control en la PC.
//   ./probar.sh   (extrae el bloque LOGICA del .ino, compila y corre)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>
#include "logica_extraida.h"

static const char *MOTIVOS_TXT[] = {"Reposo","Pre-circulando","Subida rapida","Sobre objetivo","Bajo objetivo","Espera 60 s","Congelamiento","Sin hielo/bomba","Falla sensor","Manual","Apagado","Carga alta","Fuga de agua"};
static int fallos = 0, pruebas = 0;
#define VERIFICA(cond, ...) do { pruebas++; if (!(cond)) { fallos++; printf("   [FALLA] " __VA_ARGS__); printf("\n"); } } while (0)

/* ---- Modelo termico grosero: sirve para ejercitar la logica, no para predecir grados reales */
struct Planta {
  double cam = 25, tubo = 25, agua = 14, hojaSensor = 14, cuarto = 25;
  double carga = 0;                       // W de los focos
  double hielo = 0;                       // J de hielo (334 kJ por kg)
  double sesgoTubo = 0, sesgoAgua = 0;    // fallas inyectadas
  void paso(double dt, bool A, bool B, bool bomba) {
    const double C_cam = 700, C_tubo = 150, C_agua = 12500;
    double nCel = (A ? 1 : 0) + (B ? 1 : 0);
    double qhot = 0, qc = 0;
    for (int i = 0; i < (int)nCel; i++) {
      double c = 22.0 - 0.5 * (hojaSensor - tubo);
      if (c < 0) c = 0;
      qc += c; qhot += c + 12.0;
    }
    double qcond = (nCel == 0) ? 0.4 * (hojaSensor - tubo) : 0;          // celda apagada conduce un poco
    double G_air = bomba ? 4.0 : 1.0;                                    // el ventilador comparte circuito
    double q_ct = G_air * (cam - tubo);
    cam  += dt * (carga - q_ct - 2.5 * (cam - cuarto)) / C_cam;
    tubo += dt * (q_ct - qc + qcond) / C_tubo;
    double qbomba = bomba ? 6.0 : 0.0;
    double qnet = qhot + qbomba - 0.5 * (agua - cuarto) - qcond;
    if (hielo > 0 && qnet > 0) {                 // mientras haya hielo el agua no pasa de ~8 C
      hielo -= qnet * dt; agua = 8.0;
      if (hielo < 0) { agua += (-hielo) / C_agua; hielo = 0; }
    } else agua += dt * qnet / C_agua;
    hojaSensor = agua + 0.12 * qhot;
  }
};

struct Registro { uint32_t t; Salidas s; double cam, tubo, agua; };

struct Corrida {
  Control c = {};
  Planta p;
  Entradas e;
  std::mt19937 rng{42};
  std::normal_distribution<double> ruido{0.0, 0.15};
  double sigma = 0.0;
  uint32_t t = 0;                         // ms
  std::vector<Registro> log;
  bool tuboNaN = false, aguaNaN = false, camNaN = false;
  double tuboForzado = NAN, aguaForzada = NAN;   // lo que marca el sensor, sin importar el modelo

  // invariantes
  int violDwell = 0, violBomba = 0, violSeguridad = 0, transiciones = 0;
  bool Aant = false, Bant = false; uint32_t tOnA = 0, tOffA = 0, tOnB = 0, tOffB = 0;
  uint32_t bombaDesde = 0; bool bombaAnt = false;
  uint8_t maxCiclos = 0;

  Corrida() { e = { NAN, NAN, NAN, 28.0f, MODO_AUTO, false, false, false }; }

  Salidas tick() {
    t += 2000;
    p.paso(2.0, Aant, Bant, bombaAnt);
    e.tCamara = camNaN ? NAN : (float)(p.cam + (sigma > 0 ? ruido(rng) * sigma / 0.15 * 0.15 : 0));
    e.tTubo   = tuboNaN ? NAN : (float)(!isnan(tuboForzado) ? tuboForzado : p.tubo + p.sesgoTubo);
    e.tAgua   = aguaNaN ? NAN : (float)(!isnan(aguaForzada) ? aguaForzada : p.hojaSensor + p.sesgoAgua);
    Salidas s = decidir(e, c, t);

    // ---- comprobar invariantes
    bool segura = (s.motivo == M_APAGADO || s.motivo == M_SENSOR || s.motivo == M_CONGELA ||
                   s.motivo == M_SIN_HIELO || s.motivo == M_FUGA) || (!isnan(e.tTubo) && e.tTubo < CONGELA_AVISO_C);
    if (s.bomba && !bombaAnt) bombaDesde = t;
    if (!s.bomba) bombaDesde = 0;
    if ((s.celdaA || s.celdaB) && (!s.bomba || ((t - bombaDesde) < T_PRECIRC_MS && t > 60000))) violBomba++;
    if ((s.celdaA || s.celdaB) && (s.congelado || s.sinHielo || s.fallas & 6)) violSeguridad++;
    if (s.celdaA && !Aant) { if (t - tOffA < T_MIN_APAGADA_MS) violDwell++; tOnA = t; transiciones++; }
    if (!s.celdaA && Aant) { if (t - tOnA < T_MIN_ENCENDIDA_MS && !segura) violDwell++; tOffA = t; transiciones++; }
    if (s.celdaB && !Bant) { if (t - tOffB < T_MIN_APAGADA_MS) violDwell++; tOnB = t; transiciones++; }
    if (!s.celdaB && Bant) { if (t - tOnB < T_MIN_ENCENDIDA_MS && !segura) violDwell++; tOffB = t; transiciones++; }
    Aant = s.celdaA; Bant = s.celdaB; bombaAnt = s.bomba;
    if (c.ciclosRapidos > maxCiclos) maxCiclos = c.ciclosRapidos;
    log.push_back({ t, s, p.cam, p.tubo, p.hojaSensor });
    return s;
  }
  void correr(double minutos) { int n = (int)(minutos * 30); for (int i = 0; i < n; i++) tick(); }
  void ticks(int n) { for (int i = 0; i < n; i++) tick(); }
  int cambios() const { return transiciones; }
  int nivelFinal() const { return log.back().s.nivel; }
  double maxCam(uint32_t desdeMs = 0) const { double m = -1e9; for (auto &r : log) if (r.t >= desdeMs && r.cam > m) m = r.cam; return m; }
};

static void volcado(const Corrida &r, double desdeMin, double hastaMin, double cadaMin) {
  uint32_t ult = 0;
  for (auto &x : r.log) {
    double m = x.t / 60000.0;
    if (m < desdeMin || m > hastaMin || x.t < ult + cadaMin * 60000) continue;
    ult = x.t;
    printf("     %6.1f min  cam %5.2f  tubo %5.2f  agua %5.1f | pend %+5.2f pred %5.2f | N%d A%d B%d | %s\n",
           m, x.cam, x.tubo, x.agua, x.s.pendiente, x.s.prediccion, x.s.nivel, x.s.celdaA, x.s.celdaB, MOTIVOS_TXT[x.s.motivo]);
  }
}
static void seccion(const char *n) { printf("\n== %s\n", n); }

int main() {
  // ---------------------------------------------------------------- 1
  seccion("1. Arranque: nada de corriente en los primeros 60 s, aunque la camara este caliente");
  { Corrida r; r.p.cam = 38; r.p.carga = 40; r.correr(2.5);
    uint32_t primera = 0; for (auto &x : r.log) if (x.s.celdaA || x.s.celdaB) { primera = x.t; break; }
    printf("   primera celda encendida a los %u s\n", primera / 1000);
    VERIFICA(primera >= 60000, "encendio antes de 60 s (%u ms)", primera);
    VERIFICA(r.violBomba == 0 && r.violDwell == 0, "violo bomba/dwell"); }

  // ---------------------------------------------------------------- 2
  seccion("2. Perfil de carga: reposo -> 14 W -> 38 W -> 14 W -> 0 W (objetivo 28 C)");
  { Corrida r; r.p.cam = 25; r.p.hielo = 600000;
    const struct { double min, w; } fases[] = { {10, 0}, {30, 14}, {30, 38}, {30, 14}, {30, 0} };
    double tfase = 0; int idx = 0; uint8_t nivMax = 1; 
    printf("   %-9s %-8s %-9s %-10s %s\n", "t(min)", "carga W", "camara", "nivel", "motivo");
    for (auto &f : fases) {
      r.p.carga = f.w; r.correr(f.min); tfase += f.min; idx++;
      auto &u = r.log.back();
      printf("   %-9.0f %-8.0f %-9.1f %-10d %s\n", tfase, f.w, r.p.cam, u.s.nivel, MOTIVOS_TXT[u.s.motivo]);
      for (auto &x : r.log) if (x.s.nivel > nivMax) nivMax = x.s.nivel;
    }
    if (getenv("VERBOSO")) volcado(r, 40, 100, 1.0);
    printf("   transiciones de celdas: %d | maximo nivel alcanzado: %d | camara maxima tras el minuto 12: %.1f\n",
           r.transiciones, nivMax, r.maxCam(12 * 60000UL));
    VERIFICA(r.violDwell == 0, "tiempos minimos violados: %d", r.violDwell);
    VERIFICA(r.violBomba == 0, "celda con la bomba sin circular: %d", r.violBomba);
    VERIFICA(r.violSeguridad == 0, "celda encendida con proteccion activa: %d", r.violSeguridad);
    VERIFICA(r.transiciones <= 40, "demasiadas transiciones (%d): oscila", r.transiciones);
    VERIFICA(nivMax == 3, "nunca llego al nivel 3 con 38 W");
    VERIFICA(r.log.back().s.nivel == 1, "no regreso al nivel 1 sin carga"); }

  // ---------------------------------------------------------------- 3
  seccion("3. Ruido del DHT22: el ruido no debe agregar cambios al ciclo natural de la planta");
  { int base = 0, conRuido = 0;
    { Corrida r; r.p.carga = 14; r.p.cam = 28.5; r.p.hielo = 900000; r.correr(60); base = r.cambios();
      VERIFICA(r.violDwell == 0, "dwell violado sin ruido"); }
    { Corrida r; r.p.carga = 14; r.p.cam = 28.5; r.p.hielo = 900000; r.sigma = 2.0; r.correr(60); conRuido = r.cambios();
      VERIFICA(r.violDwell == 0, "dwell violado con ruido"); }
    printf("   cambios de celda en 60 min: sin ruido %d | con ruido de 0.3 C %d\n", base, conRuido);
    VERIFICA(conRuido <= base + 4, "el ruido agrega cambios: %d contra %d", conRuido, base); }

  // ---------------------------------------------------------------- 4
  seccion("4. Congelamiento: el bloque cae bajo 0.5 C -> celdas fuera de inmediato y no vuelven hasta 6 C");
  { Corrida r; r.p.carga = 38; r.p.cam = 33; r.p.hielo = 600000; r.correr(6);
    VERIFICA(r.log.back().s.celdaA || r.log.back().s.celdaB, "las celdas no estaban trabajando antes de la falla");
    r.tuboForzado = -3.0; Salidas s = r.tick();
    VERIFICA(!s.celdaA && !s.celdaB, "siguio con celdas encendidas bajo 0.5 C");
    VERIFICA(s.alerta == A_CONGELA, "no marco alerta de congelamiento");
    r.tuboForzado = 4.0; r.correr(4);                                   // 4 C durante 4 min: no rearma
    bool prendio = false; for (size_t i = r.log.size() - 120; i < r.log.size(); i++) if (r.log[i].s.celdaA || r.log[i].s.celdaB) prendio = true;
    VERIFICA(!prendio, "volvio a encender con el bloque a 4 C (histeresis)");
    r.tuboForzado = 7.0; r.correr(3);
    prendio = false; for (size_t i = r.log.size() - 90; i < r.log.size(); i++) if (r.log[i].s.celdaA || r.log[i].s.celdaB) prendio = true;
    VERIFICA(prendio, "no volvio a trabajar al calentar a 7 C");
    VERIFICA(r.violDwell == 0 && r.violSeguridad == 0, "violo tiempos o seguridad"); }

  // ---------------------------------------------------------------- 5
  seccion("5. Se acaba el hielo: waterblock llega a 30 C -> corte; con cada corte exige mas frio para volver");
  { Corrida r; r.p.carga = 38; r.p.cam = 33; r.p.hielo = 600000; r.correr(5);
    r.aguaForzada = 31; Salidas s = r.tick();
    VERIFICA(!s.celdaA && !s.celdaB, "siguio encendido a 31 C");
    VERIFICA(s.alerta == A_SIN_HIELO, "sin alerta de hielo");
    r.aguaForzada = 23; r.correr(4);
    VERIFICA(!r.log.back().s.celdaA && !r.log.back().s.celdaB, "rearmo a 23 C con un corte previo (pide 21)");
    r.aguaForzada = 20; r.correr(4);
    VERIFICA(r.log.back().s.celdaA || r.log.back().s.celdaB, "no volvio al bajar a 20 C");
    r.aguaForzada = 31; r.ticks(2);                                    // segundo corte
    r.aguaForzada = 20; r.correr(4);
    VERIFICA(!r.log.back().s.celdaA && !r.log.back().s.celdaB, "tras el segundo corte rearmo a 20 C (pide 18)");
    r.aguaForzada = 10; r.correr(4);
    VERIFICA(r.log.back().s.celdaA || r.log.back().s.celdaB, "no volvio con hielo nuevo (10 C)");
    VERIFICA(r.violDwell == 0 && r.violSeguridad == 0, "violo tiempos o seguridad"); }

  // ---------------------------------------------------------------- 6
  seccion("6. Sensores muertos");
  { Corrida r; r.p.carga = 38; r.p.cam = 33; r.correr(5);
    r.tuboNaN = true; r.ticks(4);                                       // 8 s: todavia en tolerancia
    VERIFICA(r.log.back().s.celdaA || r.log.back().s.celdaB, "apago antes de los 10 s de tolerancia");
    r.ticks(3);                                                        // 14 s: ya paso de 10
    VERIFICA(!r.log.back().s.celdaA && !r.log.back().s.celdaB, "siguio encendido con el sensor del bloque muerto");
    VERIFICA(r.log.back().s.alerta == A_SENSOR && (r.log.back().s.fallas & 2), "alerta/bit de sensor incorrecto");
    r.tuboNaN = false; r.correr(3);
    VERIFICA(r.log.back().s.alerta == A_NINGUNA, "la alerta no se limpio al volver el sensor");
    VERIFICA(r.log.back().s.motivo != M_SENSOR, "el motivo sigue diciendo falla de sensor tras recuperarse");
    r.aguaNaN = true; r.correr(0.8);
    VERIFICA(!r.log.back().s.celdaA && !r.log.back().s.celdaB && (r.log.back().s.fallas & 4), "siguio sin sensor de agua");
    r.aguaNaN = false;
    r.camNaN = true; r.ticks(12);                                      // 24 s
    VERIFICA(r.log.back().s.fallas == 0, "marco falla de camara antes de 30 s");
    r.ticks(6);                                                        // 36 s
    VERIFICA(r.log.back().s.fallas & 1, "no marco falla de camara tras 30 s"); }

  // ---------------------------------------------------------------- 7
  seccion("7. Modo apagado: celdas fuera ya, bomba 60 s despues");
  { Corrida r; r.p.carga = 38; r.p.cam = 33; r.correr(5);
    r.e.modo = MODO_APAGADO; Salidas s = r.tick();
    VERIFICA(!s.celdaA && !s.celdaB, "celdas siguieron con modo apagado");
    VERIFICA(s.bomba, "la bomba se apago de golpe (sin post-circulacion)");
    r.correr(1.2);
    VERIFICA(!r.log.back().s.bomba, "la bomba sigue tras 72 s apagado");
    r.e.modo = MODO_AUTO; r.correr(1);
    VERIFICA(r.log.back().s.bomba, "la bomba no volvio al reanudar"); }

  // ---------------------------------------------------------------- 8
  seccion("8. Modo manual con interruptores de la app, tecleados rapido");
  { Corrida r; r.p.cam = 25; r.p.carga = 0; r.e.modo = MODO_MANUAL; r.correr(1.2);
    r.e.manualA = true; r.correr(0.3);
    VERIFICA(r.log.back().s.celdaA, "no obedecio manual A");
    for (int i = 0; i < 6; i++) { r.e.manualA = !r.e.manualA; r.correr(0.2); }   // juega con el interruptor
    VERIFICA(r.violDwell == 0, "el manual rompio los tiempos minimos: %d", r.violDwell); }

  // ---------------------------------------------------------------- 9
  seccion("9. Carga avisada (anticipacion): con la camara a 27.8 C y objetivo 28, solo se enciende si se avisa");
  { Corrida sin, con;
    for (Corrida *r : { &sin, &con }) { r->p.cam = 27.8; r->p.carga = 14.2; r->p.hielo = 600000; r->correr(2); }
    con.e.cargaAlta = true;
    sin.correr(1.0); con.correr(1.0);
    if (getenv("VERBOSO")) { printf("   -- con aviso --\n"); volcado(con, 0, 4, 0.25); }
    printf("   sin aviso: nivel %d (%s) | con aviso: nivel %d (%s)\n", sin.log.back().s.nivel,
           MOTIVOS_TXT[sin.log.back().s.motivo], con.log.back().s.nivel, MOTIVOS_TXT[con.log.back().s.motivo]);
    VERIFICA(sin.log.back().s.nivel == 1, "sin aviso no debia encender");
    VERIFICA(con.log.back().s.nivel >= 2, "con aviso de carga no se anticipo"); }

  // ---------------------------------------------------------------- 11
  seccion("11. Carga de 34 W (al borde de lo que aguantan las celdas): detecta el ciclado y ensancha la banda");
  { Corrida r; r.p.cam = 28; r.p.carga = 34; r.p.hielo = 900000; r.correr(60);
    int cambios = r.cambios();
    printf("   cambios de celda en 60 min: %d | ciclos rapidos detectados (max): %d | camara minima tras 15 min: %.1f\n",
           cambios, r.maxCiclos, [&]{ double m = 1e9; for (auto &x : r.log) if (x.t > 900000 && x.cam < m) m = x.cam; return m; }());
    VERIFICA(r.maxCiclos >= 1, "nunca detecto el ciclado rapido");
    VERIFICA(cambios <= 24, "cicla demasiado (%d cambios/h; sin la banda adaptativa son ~64)", cambios);
    VERIFICA(r.violDwell == 0, "violo tiempos minimos"); }

  // ---------------------------------------------------------------- 12
  seccion("12. Sin termometro en el agua: no marca falla, trabaja, y deduce la falta de hielo del bloque");
  { Corrida r; r.e.sinSensorAgua = true; r.aguaNaN = true;
    r.p.carga = 38; r.p.cam = 33; r.p.hielo = 900000; r.correr(8);
    VERIFICA((r.log.back().s.fallas & 4) == 0, "marco falla por el sensor de agua ausente");
    bool trabajo = false; for (auto &x : r.log) if (x.s.celdaA || x.s.celdaB) trabajo = true;
    VERIFICA(trabajo, "las celdas nunca encendieron sin sensor de agua");
    VERIFICA(!r.log.back().s.sinHielo, "dedujo falta de hielo con el sistema enfriando bien");
    // se para la bomba: el bloque deja de enfriar y queda a la temperatura de la camara
    r.tuboForzado = (float)r.p.cam; r.correr(0.5);         // la celda ya llevaba >3 min: corta al momento
    VERIFICA(r.log.back().s.sinHielo, "no dedujo la falta de hielo con el bloque igual que la camara");
    VERIFICA(!r.log.back().s.celdaA && !r.log.back().s.celdaB, "siguio con celdas tras deducir falta de hielo");
    r.correr(3.5);                                           // 4 min despues del corte
    VERIFICA(r.log.back().s.sinHielo, "rearmo antes de 5 min");
    r.tuboForzado = NAN; r.correr(1.5);                       // 5.5 min despues
    VERIFICA(!r.log.back().s.sinHielo, "no reintento despues de 5 min");
    VERIFICA(r.violDwell == 0 && r.violSeguridad == 0, "violo tiempos o seguridad"); }

  // ---------------------------------------------------------------- 13
  seccion("13. Fuga junto a la bomba: se confirma en 4 s, apaga todo y vuelve 30 s despues de secarse");
  { FiltroFuga f = {};
    VERIFICA(!filtrarFuga(f, NAN, 0), "sin sensor marco fuga");
    VERIFICA(!filtrarFuga(f, 90, 1000) && !filtrarFuga(f, 90, 4000), "una gota de 3 s ya marco fuga");
    VERIFICA(filtrarFuga(f, 90, 5100), "no confirmo la fuga a los 4 s");
    VERIFICA(filtrarFuga(f, 35, 6000), "solto la fuga con 35 %%: dentro de la histeresis");
    VERIFICA(filtrarFuga(f, 10, 7000) && filtrarFuga(f, 10, 30000), "solto la fuga antes de 30 s seca");
    VERIFICA(!filtrarFuga(f, 10, 37100), "no solto la fuga tras 30 s seca"); }
  { Corrida r; r.e.sinSensorAgua = true; r.aguaNaN = true;
    r.p.carga = 38; r.p.cam = 33; r.p.hielo = 900000; r.correr(5);
    bool trabajaba = r.log.back().s.celdaA || r.log.back().s.celdaB;
    r.e.fuga = true; Salidas s = r.tick();
    VERIFICA(trabajaba, "las celdas no trabajaban antes de la fuga");
    VERIFICA(!s.celdaA && !s.celdaB && !s.bomba, "con fuga siguio algo encendido");
    VERIFICA(s.alerta == A_FUGA && s.motivo == M_FUGA, "no marco la alerta de fuga");
    r.e.fuga = false; r.correr(1);
    VERIFICA(r.log.back().s.bomba && r.log.back().s.alerta == A_NINGUNA, "no volvio tras secarse");
    VERIFICA(r.violSeguridad == 0, "violo la seguridad"); }

  // ---------------------------------------------------------------- 10
  seccion("10. Reparto del desgaste: la celda con menos horas es la que trabaja sola");
  { Corrida r; r.c.msOnA = 3600000; r.c.msOnB = 1000000; r.p.cam = 31; r.p.carga = 14; r.correr(3);
    bool soloB = false; for (auto &x : r.log) if (x.s.nivel == 2 && x.s.celdaB && !x.s.celdaA) { soloB = true; break; }
    VERIFICA(soloB, "en nivel 2 no eligio la celda B (menos horas)"); }

  printf("\n=============================================\n");
  printf("  %d verificaciones, %d fallas\n", pruebas, fallos);
  return fallos ? 1 : 0;
}
