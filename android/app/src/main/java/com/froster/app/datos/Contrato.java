package com.froster.app.datos;

/**
 * Rutas y códigos del contrato de datos (docs/CONTRATO_FIREBASE.md).
 * Son los mismos que usan el ESP32 y el tablero web: si cambian aquí, cambian en los tres.
 */
public final class Contrato {
    private Contrato() { }

    public static final String LECTURAS = "froster/lecturas";
    public static final String ESTADO = "froster/estado";
    public static final String CONFIG = "froster/config";
    public static final String ACK = "froster/ack";
    public static final String DISPOSITIVO = "froster/dispositivo";
    public static final String EVENTOS = "froster/eventos";

    /** Si estado/uptime_s no cambia en este tiempo, el ESP32 dejó de publicar. */
    public static final long LATIDO_MAXIMO_MS = 15000;

    /** Rango que acepta el ESP32 para config/setpoint. */
    public static final double SETPOINT_MIN = 15.0;
    public static final double SETPOINT_MAX = 40.0;

    /** Valores de estado/alerta_id. */
    public static final int ALERTA_NINGUNA = 0;
    public static final int ALERTA_CONGELAMIENTO = 1;
    public static final int ALERTA_SIN_HIELO = 2;
    public static final int ALERTA_SENSOR = 3;
    public static final int ALERTA_FUGA = 4;

    public static String nombreAlerta(int id) {
        switch (id) {
            case ALERTA_CONGELAMIENTO: return "Congelamiento del bloque";
            case ALERTA_SIN_HIELO: return "Sin hielo o bomba parada";
            case ALERTA_SENSOR: return "Falla de sensor";
            case ALERTA_FUGA: return "Fuga de agua junto a la bomba";
            default: return "Sin alertas";
        }
    }

    public static String nombreNivel(int nivel) {
        switch (nivel) {
            case 1: return "Solo bomba y ventiladores";
            case 2: return "Una celda Peltier";
            case 3: return "Las dos celdas";
            default: return "Sin dato";
        }
    }

    /** Ajusta un objetivo al rango permitido y a pasos de 0.5 °C. */
    public static double ajustarSetpoint(double valor) {
        double redondeado = Math.round(valor * 2.0) / 2.0;
        return Math.max(SETPOINT_MIN, Math.min(SETPOINT_MAX, redondeado));
    }
}
