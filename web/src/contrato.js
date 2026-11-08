// Rutas y códigos del contrato de datos (docs/CONTRATO_FIREBASE.md). Mismos que en el ESP32 y la app.
export const RUTAS = {
  lecturas: "froster/lecturas",
  estado: "froster/estado",
  config: "froster/config",
  ack: "froster/ack",
  dispositivo: "froster/dispositivo",
  eventos: "froster/eventos",
};

// estado/alerta_id
export const ALERTAS = {
  0: { nombre: "Sin alertas", grave: false },
  1: { nombre: "Congelamiento del bloque", grave: true },
  2: { nombre: "Sin hielo o bomba parada", grave: true },
  3: { nombre: "Falla de sensor", grave: true },
  4: { nombre: "Fuga de agua junto a la bomba", grave: true },
};

export const NIVELES = {
  1: "Solo bomba y ventiladores",
  2: "Una celda Peltier",
  3: "Las dos celdas",
};

// Si estado/uptime_s no cambia en este tiempo, el ESP32 no está publicando.
export const LATIDO_MAXIMO_MS = 15000;
