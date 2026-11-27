// Catálogo de ventanas del panel. El `id` es el contrato con la app Android: ella lee
// froster/ui/layout y dibuja las mismas ventanas, en el mismo orden, solo para ver.
import Gabinete from "./Gabinete";
import {
  Celdas, Control, Diagnostico, Dispositivo, Fugas, GraficaVentana, LatenciaVentana, Protecciones,
  Pruebas, Registro, Resumen, SinSensor, Sistema, Temperaturas, Ventilacion,
} from "./Ventanas";

export const CATALOGO = {
  gabinete:     { titulo: "Gabinete en vivo",            comp: Gabinete,        ancho: 2 },
  resumen:      { titulo: "Estado del sistema",          comp: Resumen,         ancho: 1 },
  diagnostico:  { titulo: "Diagnóstico por componente",  comp: Diagnostico,     ancho: 1 },
  control:      { titulo: "Objetivo y modo",             comp: Control,         ancho: 1 },
  temperaturas: { titulo: "Temperaturas y humedad",      comp: Temperaturas,    ancho: 1 },
  ventilacion:  { titulo: "Bomba y ventiladores (PWM)",  comp: Ventilacion,     ancho: 1 },
  fugas:        { titulo: "Control de fugas",            comp: Fugas,           ancho: 1 },
  grafica:      { titulo: "Gráfica de 30 min",           comp: GraficaVentana,  ancho: 3 },
  celdas:       { titulo: "Celdas Peltier",              comp: Celdas,          ancho: 1 },
  protecciones: { titulo: "Protecciones y tiempos",      comp: Protecciones,    ancho: 1 },
  pruebas:      { titulo: "Prueba de componentes",       comp: Pruebas,         ancho: 1 },
  registro:     { titulo: "Registro de eventos",         comp: Registro,        ancho: 2 },
  dispositivo:  { titulo: "ESP32: red y placa",          comp: Dispositivo,     ancho: 1 },
  latencia:     { titulo: "Latencia de comandos",        comp: LatenciaVentana, ancho: 1 },
  sistema:      { titulo: "Buzzer y calibración",        comp: Sistema,         ancho: 1 },
  sin_sensor:   { titulo: "Sensores no instalados",      comp: SinSensor,       ancho: 1 },
};

export const ACOMODO_FABRICA = Object.entries(CATALOGO).map(([id, v]) => ({ id, w: v.ancho, min: false }));

/** Limpia lo que venga de Firebase: ids conocidos, ancho 1–4, sin repetidos. */
export function sanear(lista) {
  if (!Array.isArray(lista)) return null;
  const vistos = new Set();
  const limpio = [];
  for (const v of lista) {
    if (!v || !CATALOGO[v.id] || vistos.has(v.id)) continue;
    vistos.add(v.id);
    limpio.push({ id: v.id, w: Math.min(4, Math.max(1, Number(v.w) || CATALOGO[v.id].ancho)), min: !!v.min });
  }
  return limpio.length ? limpio : null;
}
