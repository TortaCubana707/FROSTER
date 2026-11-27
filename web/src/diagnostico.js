// Diagnóstico por componente. Traduce lo que publica el ESP32 a "esta pieza está bien / falla, y qué
// revisar". Lo usan la ventana Diagnóstico, el gabinete animado y la app (misma lógica en Java).
import { FALLA_AGUA, FALLA_BLOQUE, FALLA_CAMARA } from "./contrato";

const num = (v) => typeof v === "number" && !Number.isNaN(v);
const f1 = (v, u = "") => (num(v) ? `${v.toFixed(1)}${u}` : "--");

/** Orden de gravedad para ordenar y para el color: falla > aviso > activo > ok > sin_sensor. */
export const GRAVEDAD = { falla: 4, aviso: 3, desconocido: 2, activo: 1, ok: 0, sin_sensor: -1 };

export function diagnosticar({ lecturas: l = {}, estado: e = {}, dispositivo: d = {}, vivo, conectado }) {
  const fallas = e.fallas ?? 0;
  const sinDatos = !vivo;
  const c = (id, nombre, estado, valor, detalle, queHacer = "") => ({ id, nombre, estado, valor, detalle, queHacer });
  const prueba = e.prueba || "";

  const lista = [
    conectado === false
      ? c("firebase", "Firebase (este navegador)", "falla", "sin conexión", "Este navegador no llega a Firebase.",
          "Revisa el internet de esta computadora.")
      : c("firebase", "Firebase (este navegador)", "ok", "conectado", "Recibiendo datos en tiempo real."),

    sinDatos
      ? c("esp32", "ESP32", "falla", "sin latido", "No publica desde hace más de 15 s.",
          "Revisa que tenga corriente (LED encendido) y Wi-Fi. Abre el Monitor Serie a 115200.")
      : c("esp32", "ESP32", num(d.temp_chip) && d.temp_chip > 75 ? "aviso" : "ok",
          `v${d.firmware ?? "?"}`, `Chip ${f1(d.temp_chip, " °C")} · memoria libre ${d.heap_libre_kb ?? "--"} KB · último reinicio: ${d.ultimo_reinicio ?? "--"}`,
          num(d.temp_chip) && d.temp_chip > 75 ? "El chip está muy caliente: dale ventilación a la caja de electrónica." : ""),

    !num(d.rssi)
      ? c("wifi", "Wi-Fi", "desconocido", "--", "Sin datos de red todavía.")
      : c("wifi", "Wi-Fi", d.rssi < -80 ? "aviso" : "ok", `${d.rssi} dBm`,
          `${d.ssid ?? ""} · canal ${d.canal ?? "--"} · ${d.caidas_wifi ?? 0} caídas`,
          d.rssi < -80 ? "Señal débil: acerca el ESP32 al router o al punto de acceso." : ""),

    fallas & FALLA_CAMARA
      ? c("dht22", "DHT22 (cámara)", "falla", "sin lectura", "La cámara no contesta desde hace más de 30 s.",
          "Revisa el cable del DHT22 en GPIO4 y su resistencia de 10 kΩ a 3.3 V.")
      : c("dht22", "DHT22 (cámara)", "ok", `${f1(l.ambiente, " °C")} · ${f1(l.humedad, " %")}`,
          `Rocío ${f1(l.punto_rocio, " °C")}`),

    fallas & FALLA_BLOQUE
      ? c("ds18b20", "DS18B20 (bloque frío)", "falla", "sin lectura", "El termómetro del bloque no contesta.",
          "Revisa el cable en GPIO18 y la resistencia de 4.7 kΩ. Sin esta lectura las celdas no encienden.")
      : e.congelado
        ? c("ds18b20", "DS18B20 (bloque frío)", "falla", f1(l.placa_fria, " °C"), "Congelamiento: celdas apagadas.",
            "Espera a que el bloque se caliente al rearme. Si pasa seguido, sube el corte o baja el nivel.")
        : c("ds18b20", "DS18B20 (bloque frío)", e.condensa ? "aviso" : "ok", f1(l.placa_fria, " °C"),
            e.condensa ? "Bajo el punto de rocío: está condensando." : "Lectura normal.",
            e.condensa ? "Revisa que el agua condensada no gotee sobre la electrónica." : ""),

    fallas & FALLA_AGUA
      ? c("agua", "DS18B20 (waterblock)", "falla", "sin lectura", "El termómetro del agua no contesta.", "Revisa el cable en GPIO5.")
      : num(l.agua)
        ? c("agua", "DS18B20 (waterblock)", "ok", f1(l.agua, " °C"), "Temperatura del agua.")
        : c("agua", "DS18B20 (waterblock)", "sin_sensor", "no instalado",
            "La falta de hielo se deduce del bloque frío.", "Para medirlo: DS18B20 en GPIO5 y SENSOR_AGUA 1 en el firmware."),

    e.fuga
      ? c("fuga", "Sensor de fuga", "falla", `${l.fuga_pct ?? "--"} %`, "FUGA: bomba y celdas cortadas.",
          "Desconecta, seca la zona de la bomba y revisa mangueras y abrazaderas. Todo vuelve 30 s después de secarse.")
      : num(l.fuga_pct)
        ? c("fuga", "Sensor de fuga", l.fuga_pct >= 20 ? "aviso" : "ok", `${l.fuga_pct} %`,
            `ADC ${l.fuga_raw ?? "--"} (seco ≈ 4095)`, l.fuga_pct >= 20 ? "Hay humedad cerca de la bomba: revisa antes de que corte." : "")
        : c("fuga", "Sensor de fuga", "desconocido", "--", "Sin lectura."),

    e.fuga
      ? c("bomba", "Bomba", "falla", "cortada", "Apagada por la fuga.", "Resuelve la fuga primero.")
      : e.sin_hielo
        ? c("bomba", "Bomba", "falla", e.bomba ? "encendida" : "apagada",
            "El bloque no enfría aunque hay celdas: no hay hielo o la bomba no mueve agua.",
            "Revisa que haya hielo en la cubeta, que la bomba esté sumergida y que la manguera no esté doblada.")
        : c("bomba", "Bomba", prueba === "bomba" ? "activo" : e.bomba ? "activo" : "ok",
            e.bomba ? `${e.pwm_fans ?? "--"} %` : "apagada", e.pwm_auto === false ? "PWM manual" : "PWM por nivel"),

    c("ventiladores", "Ventiladores (3)", e.fuga ? "falla" : e.bomba ? "activo" : "ok",
      e.bomba ? `${e.pwm_fans ?? "--"} % · ${e.pwm_hz ?? "--"} Hz` : "apagados",
      "Comparten el MOSFET de GPIO27 con la bomba.", e.fuga ? "Cortados por la fuga." : ""),

    c("celda_a", "Celda Peltier A", e.celda_a ? "activo" : "ok", e.celda_a ? "encendida" : "apagada",
      `${num(e.horas_a) ? e.horas_a.toFixed(1) : "--"} h de uso${prueba === "celda_a" ? " · EN PRUEBA" : ""}`),
    c("celda_b", "Celda Peltier B", e.celda_b ? "activo" : "ok", e.celda_b ? "encendida" : "apagada",
      `${num(e.horas_b) ? e.horas_b.toFixed(1) : "--"} h de uso${prueba === "celda_b" ? " · EN PRUEBA" : ""}`),

    c("buzzer", "Buzzer", e.silencio ? "aviso" : "ok", e.silencio ? "silenciado" : "activo",
      e.silencio ? "No sonará ni en las alertas." : "Suena en cambios de nivel y alertas.",
      e.silencio ? "Recuerda quitar el silencio al terminar." : ""),

    c("memoria", "Memoria del ESP32", num(d.heap_min_kb) && d.heap_min_kb < 20 ? "aviso" : num(d.heap_libre_kb) ? "ok" : "desconocido",
      num(d.heap_libre_kb) ? `${d.heap_libre_kb} KB libres` : "--", `Mínimo desde el arranque: ${d.heap_min_kb ?? "--"} KB`,
      num(d.heap_min_kb) && d.heap_min_kb < 20 ? "Poca memoria: puede reiniciarse solo. Reporta el último motivo de reinicio." : ""),
  ];

  // Sin latido, nada de lo que dice el ESP32 es actual
  if (sinDatos) for (const x of lista) if (!["firebase", "esp32"].includes(x.id) && x.estado !== "sin_sensor") x.estado = "desconocido";

  const peor = lista.reduce((m, x) => Math.max(m, GRAVEDAD[x.estado]), 0);
  return {
    componentes: lista,
    porId: Object.fromEntries(lista.map((x) => [x.id, x])),
    fallas: lista.filter((x) => x.estado === "falla"),
    avisos: lista.filter((x) => x.estado === "aviso"),
    general: peor >= 4 ? "falla" : peor >= 3 ? "aviso" : peor >= 2 ? "desconocido" : "ok",
  };
}
