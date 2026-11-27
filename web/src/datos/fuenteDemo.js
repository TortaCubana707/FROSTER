// Fuente de datos SIMULADA para revisar el diseño sin el prototipo (abrir con ?demo).
// Imita al ESP32: un modelo térmico simple, decisión por niveles, eventos, ack de comandos
// y alertas. Todo lo que muestra va marcado como DEMO: NO sirve como evidencia de la entrega.
import { AJUSTES, ALERTAS, ajustarSetpoint } from "../contrato";

const PASO_MS = 2000;
const parametros = new URLSearchParams(window.location.search);
const alertaForzada = Number(parametros.get("alerta")) || 0;

const ahora = () => Date.now();
const redondear = (v, d = 1) => Math.round(v * 10 ** d) / 10 ** d;
let siguienteId = 0;
const idEvento = () => `-DEMO${String(ahora()).slice(-9)}${String(siguienteId++).padStart(3, "0")}`;

const datos = {
  lecturas: {
    ambiente: 27.6, humedad: 62, punto_rocio: 19.6, placa_fria: 12.4, fuga_pct: 0,
    pendiente: -0.08, prediccion: 27.4, ts: ahora(),
  },
  estado: {
    nivel: 2, celda_a: true, celda_b: false, bomba: true, pwm_fans: 86, setpoint: 26,
    modo: "auto", motivo: "Sobre objetivo", alerta: "", alerta_id: 0, condensa: false,
    carga_alta: false, uptime_s: 1834,
    pwm_auto: true, pwm_pct_manual: 70, pwm_hz: 1000, silencio: false, fallas: 0, congelado: false,
    sin_hielo: false, esperando: false, fuga: false, horas_a: 41.3, horas_b: 38.9, prueba: "", prueba_resultado: "",
    ajustes: Object.fromEntries(AJUSTES.map((a) => [a.clave, a.fabrica])),
  },
  config: { setpoint: 26, modo: "auto", manual_a: false, manual_b: false, carga_alta: false, pwm_auto: true, pwm_pct: 70, silencio: false },
  ack: {},
  dispositivo: {
    ip: "192.168.100.24", rssi: -51, firmware: "2.1 (DEMO)", caidas_wifi: 0, ultima_conexion: ahora(),
    ssid: "LabSistemas", canal: 6, mac: "DE:M0:00:00:00:01", heap_libre_kb: 168, heap_min_kb: 141, temp_chip: 47.5,
    cpu_mhz: 240, sketch_kb: 1360, sketch_libre_kb: 1920, ultimo_reinicio: "encendido",
  },
  ui: {},
  eventos: [
    { tipo: "nivel", texto: "Nivel 1: Bajo objetivo", nivel: 1, camara: 25.8, setpoint: 26, ts: ahora() - 21 * 60000 },
    { tipo: "pico", texto: "Camara sobre el objetivo + 2.5 C", nivel: 3, camara: 28.6, setpoint: 26, ts: ahora() - 14 * 60000 },
    { tipo: "nivel", texto: "Nivel 3: Sobre objetivo", nivel: 3, camara: 28.7, setpoint: 26, ts: ahora() - 14 * 60000 + 2000 },
    { tipo: "nivel", texto: "Nivel 2: Sobre objetivo", nivel: 2, camara: 27.9, setpoint: 26, ts: ahora() - 6 * 60000 },
  ].map((e) => ({ id: idEvento(), ...e })),
};

const oyentes = new Map();   // ruta -> Set de { alCambiar, ultimos }
let fugaHasta = 0;
let pruebaHasta = 0;

function valorDe(ruta, ultimos) {
  if (ruta === ".info/connected") return true;
  const clave = ruta.replace(/^froster\//, "");
  if (clave === "ui") return structuredClone(datos.ui);
  if (clave === "eventos") return datos.eventos.slice(-(ultimos ?? datos.eventos.length));
  return structuredClone(datos[clave]);
}

function avisar(clave) {
  const ruta = `froster/${clave}`;
  oyentes.get(ruta)?.forEach((o) => o.alCambiar(valorDe(ruta, o.ultimos)));
}

function agregarEvento(tipo, texto) {
  const { nivel, setpoint } = datos.estado;
  datos.eventos.push({ id: idEvento(), tipo, texto, nivel, camara: datos.lecturas.ambiente, setpoint, ts: ahora() });
  datos.eventos = datos.eventos.slice(-50);
  avisar("eventos");
}

// Un paso del "ESP32": decide el nivel y mueve la temperatura de la cámara.
function paso() {
  const l = datos.lecturas, e = datos.estado, c = datos.config;
  const antes = { nivel: e.nivel, alerta: e.alerta_id, pico: l.ambiente > e.setpoint + 2.5 };

  const alerta = alertaForzada || (ahora() < fugaHasta ? 4 : 0);
  e.alerta_id = alerta;
  e.alerta = alerta ? ALERTAS[alerta].nombre : "";
  l.fuga_pct = alerta === 4 ? 87 : 0;
  l.fuga_raw = alerta === 4 ? 1960 : 4095;
  e.fuga = alerta === 4;
  e.fallas = alerta === 3 ? 1 : 0;
  if (e.prueba && ahora() > pruebaHasta) { e.prueba_resultado = `Prueba de ${e.prueba} terminada`; e.prueba = ""; }

  if (alerta) {
    Object.assign(e, { nivel: 1, celda_a: false, celda_b: false, bomba: alerta !== 4, motivo: "Proteccion activa" });
  } else if (e.modo === "off") {
    Object.assign(e, { nivel: 1, celda_a: false, celda_b: false, bomba: false, motivo: "Apagado" });
  } else if (e.modo === "manual") {
    const n = 1 + Number(c.manual_a) + Number(c.manual_b);
    Object.assign(e, { nivel: n, celda_a: c.manual_a, celda_b: c.manual_b, bomba: true, motivo: "Manual" });
  } else {
    const n = l.ambiente > e.setpoint + 2.5 ? 3 : l.ambiente > e.setpoint ? 2 : 1;
    Object.assign(e, { nivel: n, celda_a: n >= 2, celda_b: n === 3, bomba: true,
      motivo: n === 1 ? "Bajo objetivo" : "Sobre objetivo" });
  }
  const aj = e.ajustes;
  e.pwm_fans = e.prueba === "bomba" ? 100 : !e.bomba ? 0 : !e.pwm_auto
    ? Math.max(c.pwm_pct, e.celda_a || e.celda_b ? 50 : 0)
    : e.nivel === 3 ? aj.pwm_n3 : e.nivel === 2 ? aj.pwm_n2 : aj.pwm_n1;
  if (e.prueba === "celda_a") e.celda_a = true;
  if (e.prueba === "celda_b") e.celda_b = true;
  e.horas_a = redondear(e.horas_a + (e.celda_a ? PASO_MS / 3600000 : 0), 3);
  e.horas_b = redondear(e.horas_b + (e.celda_b ? PASO_MS / 3600000 : 0), 3);

  const celdas = Number(e.celda_a) + Number(e.celda_b);
  const calorFoco = e.carga_alta ? 0.16 : 0.1;
  const enfriamiento = 0.04 + celdas * 0.09;
  const anterior = l.ambiente;
  l.ambiente = redondear(anterior + calorFoco - enfriamiento + (Math.random() - 0.5) * 0.03, 2);
  l.placa_fria = redondear(l.placa_fria + ((22 - celdas * 7) - l.placa_fria) * 0.1, 2);
  // Pendiente suavizada (°C/min), como la que calcula el firmware con varias lecturas
  l.pendiente = redondear(l.pendiente * 0.8 + (l.ambiente - anterior) * (60000 / PASO_MS) * 0.2, 2);
  l.prediccion = redondear(l.ambiente + l.pendiente * 2, 1);
  l.humedad = redondear(61 + (Math.random() - 0.5) * 2, 1);
  l.ts = ahora();
  e.condensa = l.placa_fria < l.punto_rocio;
  e.uptime_s += PASO_MS / 1000;

  if (e.nivel !== antes.nivel) agregarEvento("nivel", `Nivel ${e.nivel}: ${e.motivo}`);
  if (e.alerta_id !== antes.alerta) agregarEvento("alerta", e.alerta_id ? e.alerta : "Alerta resuelta");
  if (!antes.pico && l.ambiente > e.setpoint + 2.5) agregarEvento("pico", "Camara sobre el objetivo + 2.5 C");
  avisar("lecturas");
  avisar("estado");
}

// La simulación arranca con la primera escucha: si la app usa Firebase, este código nunca corre.
let reloj = null;
const arrancar = () => { reloj ??= setInterval(paso, PASO_MS); };

// 30 minutos de historia inventada, solo para que la gráfica del modo DEMO no empiece vacía.
function historialInicial() {
  const puntos = [];
  const fin = ahora();
  for (let t = fin - 30 * 60000; t < fin; t += 20000) {
    const fase = (fin - t) / 60000;
    puntos.push({
      t,
      camara: redondear(27.6 + Math.sin(fase / 4) * 1.4 + (Math.random() - 0.5) * 0.15, 2),
      bloque: redondear(12.4 + Math.sin(fase / 5) * 3, 2),
      objetivo: 26,
    });
  }
  return puntos;
}

export const fuenteDemo = {
  demo: true,

  escuchar(ruta, alCambiar, { ultimos } = {}) {
    arrancar();
    const o = { alCambiar, ultimos };
    if (!oyentes.has(ruta)) oyentes.set(ruta, new Set());
    oyentes.get(ruta).add(o);
    setTimeout(() => alCambiar(valorDe(ruta, ultimos)), 0);
    return () => oyentes.get(ruta).delete(o);
  },

  // Igual que el ESP32 real: aplica lo que llegó a config/ y lo confirma en ack/ con el mismo cmd_id.
  actualizar(ruta, cambios) {
    if (ruta === "froster/ui") { Object.assign(datos.ui, cambios); avisar("ui"); return Promise.resolve(); }
    if (ruta !== "froster/config") return Promise.reject(new Error("La demo solo acepta config/ y ui/"));
    const { cmd_id: cmd, accion, accion_id: _id, ajustes, ...ordenes } = cambios;
    const e = datos.estado;
    if (ajustes) Object.assign(e.ajustes, ajustes);
    if ("pwm_auto" in ordenes) e.pwm_auto = ordenes.pwm_auto;
    if ("pwm_pct" in ordenes) e.pwm_pct_manual = ordenes.pwm_pct;
    if ("silencio" in ordenes) e.silencio = ordenes.silencio;
    if (ajustes?.pwm_hz) e.pwm_hz = ajustes.pwm_hz;
    if (accion) {
      e.prueba_resultado = "";
      if (accion.startsWith("probar_")) { e.prueba = accion.slice(7); pruebaHasta = ahora() + (accion.includes("celda") ? 10000 : 5000); }
      if (accion === "reset_horas") { e.horas_a = 0; e.horas_b = 0; e.prueba_resultado = "Horas de las celdas en cero"; }
      if (accion === "reset_caidas") { datos.dispositivo.caidas_wifi = 0; avisar("dispositivo"); }
      if (accion === "reiniciar") { e.uptime_s = 0; e.prueba_resultado = "Reiniciando..."; }
      agregarEvento("sistema", `Accion: ${accion}`);
    }
    if ("setpoint" in ordenes) ordenes.setpoint = ajustarSetpoint(ordenes.setpoint);
    Object.assign(datos.config, ordenes);
    avisar("config");
    const recibido = ahora();
    setTimeout(() => {
      for (const clave of ["setpoint", "modo", "carga_alta"]) {
        if (clave in ordenes) datos.estado[clave] = ordenes[clave];
      }
      datos.ack = { cmd_id: cmd, recibido_ms: recibido, aplicado_en_ms: 2 };
      paso();
      avisar("ack");
    }, 70 + Math.random() * 60);
    return Promise.resolve();
  },

  alCambiarSesion(alCambiar) {
    setTimeout(() => alCambiar({ email: "demo", uid: "demo" }), 0);
    return () => {};
  },
  entrar: () => Promise.resolve(),
  rol: (uid, alCambiar) => { setTimeout(() => alCambiar("admin"), 0); return () => {}; },
  salir: () => { window.location.search = ""; return Promise.resolve(); },
  historialInicial,

  /** Botón "Simular fuga" del modo DEMO: la alerta 4 dura 15 s. */
  simularFuga() {
    fugaHasta = ahora() + 15000;
    paso();
  },
};
