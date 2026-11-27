package com.froster.app.datos;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Diagnóstico por componente: la MISMA lógica que web/src/diagnostico.js del panel del
 * administrador. Traduce lo que publica el ESP32 a "esta pieza está bien / falla y qué revisar".
 * Java puro (sin Android) para poder probarlo con JUnit.
 */
public final class Diagnostico {
    private Diagnostico() { }

    public static final String FALLA = "falla", AVISO = "aviso", ACTIVO = "activo", OK = "ok",
            DESCONOCIDO = "desconocido", SIN_SENSOR = "sin_sensor";

    public static final class Componente {
        public final String id, nombre, valor, detalle, queHacer;
        public String estado;
        Componente(String id, String nombre, String estado, String valor, String detalle, String queHacer) {
            this.id = id; this.nombre = nombre; this.estado = estado;
            this.valor = valor; this.detalle = detalle; this.queHacer = queHacer == null ? "" : queHacer;
        }
    }

    public static int gravedad(String estado) {
        switch (estado) {
            case FALLA: return 4;
            case AVISO: return 3;
            case DESCONOCIDO: return 2;
            case ACTIVO: return 1;
            case SIN_SENSOR: return -1;
            default: return 0;
        }
    }

    // ------------------------------------------------------------ lectura segura de los mapas de Firebase
    public static Double num(Map<String, Object> m, String k) {
        Object v = m == null ? null : m.get(k);
        return v instanceof Number ? ((Number) v).doubleValue() : null;
    }
    public static boolean si(Map<String, Object> m, String k) {
        return m != null && Boolean.TRUE.equals(m.get(k));
    }
    public static String txt(Map<String, Object> m, String k) {
        Object v = m == null ? null : m.get(k);
        return v == null ? null : String.valueOf(v);
    }
    public static String f1(Double v, String u) {
        return v == null ? "--" : String.format(Locale.getDefault(), "%.1f%s", v, u);
    }
    private static String o(Object v) { return v == null ? "--" : String.valueOf(v); }
    private static String entero(Double v) { return v == null ? "--" : String.valueOf(Math.round(v)); }

    public static List<Componente> diagnosticar(Map<String, Object> l, Map<String, Object> e,
                                                Map<String, Object> d, boolean vivo, Boolean conectado) {
        List<Componente> x = new ArrayList<>();
        Double fallasD = num(e, "fallas");
        int fallas = fallasD == null ? 0 : fallasD.intValue();
        String prueba = txt(e, "prueba") == null ? "" : txt(e, "prueba");
        Double tempChip = num(d, "temp_chip"), rssi = num(d, "rssi"), heapLibre = num(d, "heap_libre_kb"),
                heapMin = num(d, "heap_min_kb"), fugaPct = num(l, "fuga_pct"), pwm = num(e, "pwm_fans"),
                horasA = num(e, "horas_a"), horasB = num(e, "horas_b"), agua = num(l, "agua");

        x.add(Boolean.FALSE.equals(conectado)
                ? new Componente("firebase", "Firebase (este teléfono)", FALLA, "sin conexión",
                    "El teléfono no llega a Firebase.", "Revisa el internet del teléfono.")
                : new Componente("firebase", "Firebase (este teléfono)", OK, "conectado", "Recibiendo en tiempo real.", ""));

        boolean chipCaliente = tempChip != null && tempChip > 75;
        x.add(!vivo
                ? new Componente("esp32", "ESP32", FALLA, "sin latido", "No publica desde hace más de 15 s.",
                    "Revisa que tenga corriente y Wi-Fi. Abre el Monitor Serie a 115200.")
                : new Componente("esp32", "ESP32", chipCaliente ? AVISO : OK, "v" + o(txt(d, "firmware")),
                    "Chip " + f1(tempChip, " °C") + " · memoria libre " + entero(heapLibre) + " KB · último reinicio: " + o(txt(d, "ultimo_reinicio")),
                    chipCaliente ? "El chip está muy caliente: dale ventilación a la caja de electrónica." : ""));

        x.add(rssi == null
                ? new Componente("wifi", "Wi-Fi", DESCONOCIDO, "--", "Sin datos de red todavía.", "")
                : new Componente("wifi", "Wi-Fi", rssi < -80 ? AVISO : OK, entero(rssi) + " dBm",
                    o(txt(d, "ssid")) + " · canal " + entero(num(d, "canal")) + " · " + entero(num(d, "caidas_wifi")) + " caídas",
                    rssi < -80 ? "Señal débil: acerca el ESP32 al router o al punto de acceso." : ""));

        x.add((fallas & 1) != 0
                ? new Componente("dht22", "DHT22 (cámara)", FALLA, "sin lectura", "La cámara no contesta desde hace más de 30 s.",
                    "Revisa el cable del DHT22 en GPIO4 y su resistencia de 10 kΩ a 3.3 V.")
                : new Componente("dht22", "DHT22 (cámara)", OK, f1(num(l, "ambiente"), " °C") + " · " + f1(num(l, "humedad"), " %"),
                    "Rocío " + f1(num(l, "punto_rocio"), " °C"), ""));

        boolean condensa = si(e, "condensa");
        if ((fallas & 2) != 0) {
            x.add(new Componente("ds18b20", "DS18B20 (bloque frío)", FALLA, "sin lectura", "El termómetro del bloque no contesta.",
                    "Revisa el cable en GPIO18 y la resistencia de 4.7 kΩ. Sin esta lectura las celdas no encienden."));
        } else if (si(e, "congelado")) {
            x.add(new Componente("ds18b20", "DS18B20 (bloque frío)", FALLA, f1(num(l, "placa_fria"), " °C"), "Congelamiento: celdas apagadas.",
                    "Espera a que el bloque se caliente al rearme. Si pasa seguido, sube el corte o baja el nivel."));
        } else {
            x.add(new Componente("ds18b20", "DS18B20 (bloque frío)", condensa ? AVISO : OK, f1(num(l, "placa_fria"), " °C"),
                    condensa ? "Bajo el punto de rocío: está condensando." : "Lectura normal.",
                    condensa ? "Revisa que el agua condensada no gotee sobre la electrónica." : ""));
        }

        if ((fallas & 4) != 0) {
            x.add(new Componente("agua", "DS18B20 (waterblock)", FALLA, "sin lectura", "El termómetro del agua no contesta.", "Revisa el cable en GPIO5."));
        } else if (agua != null) {
            x.add(new Componente("agua", "DS18B20 (waterblock)", OK, f1(agua, " °C"), "Temperatura del agua.", ""));
        } else {
            x.add(new Componente("agua", "DS18B20 (waterblock)", SIN_SENSOR, "no instalado",
                    "La falta de hielo se deduce del bloque frío.", "Para medirlo: DS18B20 en GPIO5 y SENSOR_AGUA 1 en el firmware."));
        }

        boolean fuga = si(e, "fuga");
        if (fuga) {
            x.add(new Componente("fuga", "Sensor de fuga", FALLA, entero(fugaPct) + " %", "FUGA: bomba y celdas cortadas.",
                    "Desconecta, seca la zona de la bomba y revisa mangueras y abrazaderas. Todo vuelve 30 s después de secarse."));
        } else if (fugaPct != null) {
            x.add(new Componente("fuga", "Sensor de fuga", fugaPct >= 20 ? AVISO : OK, entero(fugaPct) + " %",
                    "ADC " + entero(num(l, "fuga_raw")) + " (seco ≈ 4095)",
                    fugaPct >= 20 ? "Hay humedad cerca de la bomba: revisa antes de que corte." : ""));
        } else {
            x.add(new Componente("fuga", "Sensor de fuga", DESCONOCIDO, "--", "Sin lectura.", ""));
        }

        boolean bomba = si(e, "bomba");
        if (fuga) {
            x.add(new Componente("bomba", "Bomba", FALLA, "cortada", "Apagada por la fuga.", "Resuelve la fuga primero."));
        } else if (si(e, "sin_hielo")) {
            x.add(new Componente("bomba", "Bomba", FALLA, bomba ? "encendida" : "apagada",
                    "El bloque no enfría aunque hay celdas: no hay hielo o la bomba no mueve agua.",
                    "Revisa que haya hielo en la cubeta, que la bomba esté sumergida y que la manguera no esté doblada."));
        } else {
            x.add(new Componente("bomba", "Bomba", bomba || "bomba".equals(prueba) ? ACTIVO : OK,
                    bomba ? entero(pwm) + " %" : "apagada", Boolean.FALSE.equals(e == null ? null : e.get("pwm_auto")) ? "PWM manual" : "PWM por nivel", ""));
        }

        x.add(new Componente("ventiladores", "Ventiladores (3)", fuga ? FALLA : bomba ? ACTIVO : OK,
                bomba ? entero(pwm) + " % · " + entero(num(e, "pwm_hz")) + " Hz" : "apagados",
                "Comparten el MOSFET de GPIO27 con la bomba.", fuga ? "Cortados por la fuga." : ""));

        x.add(new Componente("celda_a", "Celda Peltier A", si(e, "celda_a") ? ACTIVO : OK, si(e, "celda_a") ? "encendida" : "apagada",
                f1(horasA, " h de uso") + ("celda_a".equals(prueba) ? " · EN PRUEBA" : ""), ""));
        x.add(new Componente("celda_b", "Celda Peltier B", si(e, "celda_b") ? ACTIVO : OK, si(e, "celda_b") ? "encendida" : "apagada",
                f1(horasB, " h de uso") + ("celda_b".equals(prueba) ? " · EN PRUEBA" : ""), ""));

        boolean silencio = si(e, "silencio");
        x.add(new Componente("buzzer", "Buzzer", silencio ? AVISO : OK, silencio ? "silenciado" : "activo",
                silencio ? "No sonará ni en las alertas." : "Suena en cambios de nivel y alertas.",
                silencio ? "El administrador lo silenció desde el panel." : ""));

        boolean poca = heapMin != null && heapMin < 20;
        x.add(new Componente("memoria", "Memoria del ESP32", poca ? AVISO : heapLibre != null ? OK : DESCONOCIDO,
                heapLibre != null ? entero(heapLibre) + " KB libres" : "--", "Mínimo desde el arranque: " + entero(heapMin) + " KB",
                poca ? "Poca memoria: puede reiniciarse solo." : ""));

        if (!vivo) {
            for (Componente c : x) {
                if (!c.id.equals("firebase") && !c.id.equals("esp32") && !SIN_SENSOR.equals(c.estado)) c.estado = DESCONOCIDO;
            }
        }
        return x;
    }
}
