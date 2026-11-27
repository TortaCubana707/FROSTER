// Rutas y códigos del contrato de datos (docs/CONTRATO_FIREBASE.md). Mismos que en el ESP32 y la app.
export const RUTAS = {
  lecturas: "froster/lecturas",
  estado: "froster/estado",
  config: "froster/config",
  ack: "froster/ack",
  dispositivo: "froster/dispositivo",
  eventos: "froster/eventos",
  ui: "froster/ui",               // acomodo de ventanas: lo escribe el admin, la app lo copia
  conectado: ".info/connected",   // ruta especial de Firebase: true si este navegador está conectado
};

// estado/alerta_id. Usar el id para la lógica (color, sonido) y el texto solo para mostrar.
export const ALERTAS = {
  0: { nombre: "Sin alertas", queHacer: "" },
  1: {
    nombre: "Congelamiento del bloque",
    queHacer: "El ESP32 ya apagó las celdas. Vuelven solas cuando el bloque pasa de 6 °C.",
  },
  2: {
    nombre: "Sin hielo o bomba parada",
    queHacer: "Revisa que haya hielo en la cubeta y que la bomba circule. Las celdas se reintentan en 5 min.",
  },
  3: {
    nombre: "Falla de sensor",
    queHacer: "Revisa el cable del sensor. Las celdas quedan apagadas mientras falte la lectura.",
  },
  4: {
    nombre: "Fuga de agua junto a la bomba",
    queHacer: "Desconecta y seca la zona de la bomba y revisa las mangueras. Todo vuelve 30 s después de secarse.",
  },
};

export const NIVELES = {
  1: "Solo bomba y ventiladores",
  2: "Una celda Peltier",
  3: "Las dos celdas",
};

export const MODOS = [
  { valor: "auto", nombre: "Auto" },
  { valor: "manual", nombre: "Manual" },
  { valor: "off", nombre: "Apagado" },
];

export const TIPOS_EVENTO = {
  nivel: "Cambio de nivel",
  alerta: "Alerta",
  pico: "Pico de temperatura",
  sistema: "Sistema",
};

// config/setpoint: el ESP32 solo acepta 15–40 °C; la interfaz sube y baja de medio grado.
export const SETPOINT_MIN = 15;
export const SETPOINT_MAX = 40;
export const ajustarSetpoint = (v) =>
  Math.min(SETPOINT_MAX, Math.max(SETPOINT_MIN, Math.round(v * 2) / 2));

// Si estado/uptime_s no cambia en este tiempo, el ESP32 no está publicando.
export const LATIDO_MAXIMO_MS = 15000;

// Rúbrica de la E2: ida y vuelta de un comando por Firebase.
export const LATENCIA_OBJETIVO_MS = 500;

// ---------------------------------------------------------------------------------------------
// v2.1 · Ajustes en vivo (config/ajustes). Mismos rangos que el firmware (aplicarAjuste) y que las
// reglas de Firebase: lo que salga de aquí el ESP32 lo ignora y Firebase lo rechaza.
export const AJUSTES = [
  { clave: "congela_aviso", grupo: "congela", nombre: "Aviso de congelamiento", unidad: "°C", min: 0.5, max: 8, paso: 0.5, fabrica: 2,
    ayuda: "Con el bloque frío por debajo de esto ya no sube de nivel." },
  { clave: "congela_corte", grupo: "congela", nombre: "Corte por congelamiento", unidad: "°C", min: -2, max: 4, paso: 0.5, fabrica: 0.5,
    ayuda: "Apaga las celdas. Debe ser menor que el aviso." },
  { clave: "congela_rearme", grupo: "congela", nombre: "Rearme tras congelar", unidad: "°C", min: 2, max: 15, paso: 0.5, fabrica: 6,
    ayuda: "Las celdas vuelven cuando el bloque se calienta a esto." },
  { clave: "fuga_umbral", grupo: "fuga", nombre: "Umbral de fuga", unidad: "%", min: 10, max: 90, paso: 5, fabrica: 40,
    ayuda: "Humedad junto a la bomba que cuenta como fuga (0.3 s seguidos). Corta todo." },
  { clave: "t_min_on_s", grupo: "tiempos", nombre: "Celda encendida mínimo", unidad: "s", min: 30, max: 600, paso: 10, fabrica: 60,
    ayuda: "Evita prender y apagar las Peltier seguido: el choque térmico las daña." },
  { clave: "t_min_off_s", grupo: "tiempos", nombre: "Celda apagada mínimo", unidad: "s", min: 30, max: 600, paso: 10, fabrica: 60, ayuda: "" },
  { clave: "t_confirma_s", grupo: "tiempos", nombre: "Confirmación de cambio", unidad: "s", min: 5, max: 120, paso: 5, fabrica: 20,
    ayuda: "Un cambio de nivel debe pedirse este tiempo seguido (filtra el ruido del DHT22)." },
  { clave: "t_precirc_s", grupo: "tiempos", nombre: "Pre-circulación", unidad: "s", min: 5, max: 120, paso: 5, fabrica: 15,
    ayuda: "Agua circulando antes de dar corriente a una celda." },
  { clave: "t_postcirc_s", grupo: "tiempos", nombre: "Post-circulación", unidad: "s", min: 10, max: 600, paso: 10, fabrica: 60,
    ayuda: "La bomba sigue tras apagar, para sacar el calor que queda." },
  { clave: "pwm_n1", grupo: "pwm", nombre: "PWM en nivel 1", unidad: "%", min: 20, max: 100, paso: 5, fabrica: 71, ayuda: "Solo bomba y ventiladores." },
  { clave: "pwm_n2", grupo: "pwm", nombre: "PWM en nivel 2", unidad: "%", min: 20, max: 100, paso: 5, fabrica: 86, ayuda: "Una celda." },
  { clave: "pwm_n3", grupo: "pwm", nombre: "PWM en nivel 3", unidad: "%", min: 20, max: 100, paso: 5, fabrica: 100, ayuda: "Las dos celdas." },
  { clave: "pwm_hz", grupo: "pwm", nombre: "Frecuencia del PWM", unidad: "Hz", min: 100, max: 2000, paso: 100, fabrica: 1000,
    ayuda: "El optoacoplador PC817 del D4184 no sigue más de ~2 kHz. Baja si los ventiladores zumban." },
  { clave: "cal_camara", grupo: "cal", nombre: "Calibración cámara", unidad: "°C", min: -5, max: 5, paso: 0.1, fabrica: 0, ayuda: "Se suma a la lectura del DHT22." },
  { clave: "cal_humedad", grupo: "cal", nombre: "Calibración humedad", unidad: "%", min: -15, max: 15, paso: 0.5, fabrica: 0, ayuda: "" },
  { clave: "cal_bloque", grupo: "cal", nombre: "Calibración bloque", unidad: "°C", min: -5, max: 5, paso: 0.1, fabrica: 0,
    ayuda: "En agua con hielo el DS18B20 debe marcar 0 °C." },
];
export const ajuste = (clave) => AJUSTES.find((a) => a.clave === clave);

// Acciones de una sola vez (config/accion + accion_id). El ESP32 ignora las viejas al arrancar.
export const ACCIONES = {
  reiniciar: "Reiniciar el ESP32",
  reset_horas: "Poner en cero las horas de las celdas",
  reset_caidas: "Poner en cero las caídas de Wi-Fi",
  probar_bomba: "Probar bomba y ventiladores (5 s al 100 %)",
  probar_celda_a: "Probar celda A (10 s)",
  probar_celda_b: "Probar celda B (10 s)",
  probar_buzzer: "Probar buzzer",
  probar_led: "Probar LED RGB (rojo, verde, azul, blanco)",
};

// Bits de estado/fallas
export const FALLA_CAMARA = 1, FALLA_BLOQUE = 2, FALLA_AGUA = 4;
