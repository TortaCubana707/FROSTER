package com.froster.app.datos;

/**
 * Rutas y códigos del contrato de datos (docs/CONTRATO_FIREBASE.md).
 * Son los mismos que usan el ESP32 y el tablero web: si cambian aquí, cambian en los tres.
 * Esta clase es Java puro (sin Android) para poder probarla con JUnit en la computadora.
 */
public final class Contrato {
    private Contrato() { }

    public static final String LECTURAS = "froster/lecturas";
    public static final String ESTADO = "froster/estado";
    public static final String CONFIG = "froster/config";
    public static final String ACK = "froster/ack";
    public static final String DISPOSITIVO = "froster/dispositivo";
    public static final String EVENTOS = "froster/eventos";
    /** Acomodo de ventanas que guarda el administrador en el panel web; la app lo copia. */
    public static final String UI_LAYOUT = "froster/ui/layout";

    /** Ruta especial de Firebase: true mientras el teléfono está conectado a la base. */
    public static final String CONECTADO = ".info/connected";

    /** Si estado/uptime_s no cambia en este tiempo, el ESP32 dejó de publicar. */
    public static final long LATIDO_MAXIMO_MS = 15000;

    /** Rango que acepta el ESP32 para config/setpoint. */
    public static final double SETPOINT_MIN = 15.0;
    public static final double SETPOINT_MAX = 40.0;
    /** Paso de los botones − y + del objetivo (app y tablero). */
    public static final double PASO_SETPOINT = 0.5;

    /** Valores de config/modo y estado/modo. */
    public static final String MODO_AUTO = "auto";
    public static final String MODO_MANUAL = "manual";
    public static final String MODO_OFF = "off";

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

    /** Qué hizo el ESP32 y qué tiene que revisar la persona (protecciones del firmware). */
    public static String queHacer(int id) {
        switch (id) {
            case ALERTA_CONGELAMIENTO:
                return "El ESP32 ya apagó las celdas. Vuelven solas cuando el bloque pasa de 6 °C.";
            case ALERTA_SIN_HIELO:
                return "Revisa que haya hielo en la cubeta y que la bomba circule. "
                        + "Las celdas se reintentan después de 5 minutos.";
            case ALERTA_SENSOR:
                return "Revisa el cable del sensor. Las celdas quedan apagadas mientras falte la lectura.";
            case ALERTA_FUGA:
                return "Desconecta y seca la zona de la bomba y revisa las mangueras. "
                        + "Todo se reanuda 30 s después de que el sensor se seca.";
            default:
                return "";
        }
    }

    /**
     * Patrón de vibración por alerta (espera, vibra, espera, vibra... en ms). Sigue la misma idea
     * que el buzzer del ESP32: cada alerta se reconoce sin mirar la pantalla.
     */
    public static long[] patronVibracion(int id) {
        switch (id) {
            case ALERTA_CONGELAMIENTO: return new long[]{0, 150, 100, 150, 100, 150};   // 3 cortos
            case ALERTA_SIN_HIELO: return new long[]{0, 600, 250, 600};                 // 2 largos
            case ALERTA_SENSOR: return new long[]{0, 1000};                             // 1 muy largo
            case ALERTA_FUGA: return new long[]{0, 300, 150, 300, 150, 300, 150, 300};  // 4 alternados
            default: return new long[0];
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

    public static boolean esModoValido(String modo) {
        return MODO_AUTO.equals(modo) || MODO_MANUAL.equals(modo) || MODO_OFF.equals(modo);
    }

    public static String nombreModo(String modo) {
        if (MODO_MANUAL.equals(modo)) return "manual";
        if (MODO_OFF.equals(modo)) return "apagado";
        return "automático";
    }

    public static String nombreTipoEvento(String tipo) {
        if ("nivel".equals(tipo)) return "Cambio de nivel";
        if ("alerta".equals(tipo)) return "Alerta";
        if ("pico".equals(tipo)) return "Pico de temperatura";
        return "Evento";
    }

    /** Ajusta un objetivo al rango permitido y a pasos de 0.5 °C. */
    public static double ajustarSetpoint(double valor) {
        double redondeado = Math.round(valor * 2.0) / 2.0;
        return Math.max(SETPOINT_MIN, Math.min(SETPOINT_MAX, redondeado));
    }
}
