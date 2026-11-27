package com.froster.app.ui;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Typeface;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;

import com.froster.app.datos.Contrato;
import com.froster.app.datos.Diagnostico;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.card.MaterialCardView;

import java.text.DateFormat;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Espejo del panel del administrador. Lee froster/ui/layout (lo que el admin acomoda en el tablero
 * web) y dibuja las MISMAS ventanas, en el mismo orden y minimizadas igual. Si el admin agrega, quita o
 * mueve una ventana, la app cambia en ese momento. La única ventana con controles es "Objetivo y modo"
 * (control cruzado de la rúbrica): el resto, como los ajustes finos y las pruebas, son del administrador.
 */
public class PanelEspejo {

    /** Títulos e ids: mismos que web/src/widgets/catalogo.js. */
    static final Map<String, String> TITULOS = new LinkedHashMap<>();
    static {
        TITULOS.put("gabinete", "Gabinete en vivo");
        TITULOS.put("resumen", "Estado del sistema");
        TITULOS.put("diagnostico", "Diagnóstico por componente");
        TITULOS.put("control", "Objetivo y modo");
        TITULOS.put("temperaturas", "Temperaturas y humedad");
        TITULOS.put("ventilacion", "Bomba y ventiladores (PWM)");
        TITULOS.put("fugas", "Control de fugas");
        TITULOS.put("grafica", "Gráfica de la cámara");
        TITULOS.put("celdas", "Celdas Peltier");
        TITULOS.put("protecciones", "Protecciones y tiempos");
        TITULOS.put("pruebas", "Prueba de componentes");
        TITULOS.put("registro", "Registro de eventos");
        TITULOS.put("dispositivo", "ESP32: red y placa");
        TITULOS.put("latencia", "Latencia de comandos");
        TITULOS.put("sistema", "Buzzer y calibración");
        TITULOS.put("sin_sensor", "Sensores no instalados");
    }

    private static final class Ventana { String id; boolean min; }

    /** Lo que la ventana "Objetivo y modo" le pide a la pantalla (que manda la orden y espera el ack). */
    public interface Control {
        void cambiarObjetivo(double delta);
        void cambiarModo(String modo);
    }

    private final Activity act;
    private final LinearLayout contenedor;
    private final Map<String, LinearLayout> cuerpos = new HashMap<>();
    private final DateFormat hora = DateFormat.getTimeInstance(DateFormat.MEDIUM);
    private final ArrayDeque<double[]> historia = new ArrayDeque<>();   // {camara, objetivo}
    private List<Ventana> acomodo = new ArrayList<>();
    private GabineteView gabinete;
    private Grafica grafica;

    private Map<String, Object> lecturas = Collections.emptyMap(), estado = Collections.emptyMap(),
            config = Collections.emptyMap(), dispositivo = Collections.emptyMap();
    private List<Map<String, Object>> eventos = new ArrayList<>();
    private boolean vivo;
    private Boolean conectado;
    private List<Diagnostico.Componente> diag = new ArrayList<>();
    private Control control;
    private String ultimoComando;

    public PanelEspejo(Activity act, LinearLayout contenedor) {
        this.act = act;
        this.contenedor = contenedor;
        setAcomodo(null);
    }

    // ------------------------------------------------------------ entradas

    /** froster/ui/layout: lista de { id, w, min }. Sin acomodo guardado se usa el de fábrica. */
    @SuppressWarnings("unchecked")
    public void setAcomodo(Object crudo) {
        List<Ventana> nuevo = new ArrayList<>();
        if (crudo instanceof List) {
            for (Object o : (List<Object>) crudo) {
                if (!(o instanceof Map)) continue;
                Map<String, Object> m = (Map<String, Object>) o;
                String id = Diagnostico.txt(m, "id");
                if (id == null || !TITULOS.containsKey(id)) continue;
                Ventana v = new Ventana();
                v.id = id;
                v.min = Diagnostico.si(m, "min");
                nuevo.add(v);
            }
        }
        if (nuevo.isEmpty()) {
            for (String id : TITULOS.keySet()) { Ventana v = new Ventana(); v.id = id; nuevo.add(v); }
        }
        acomodo = nuevo;
        reconstruir();
    }

    public void setLecturas(Map<String, Object> l) {
        lecturas = l;
        Double cam = Diagnostico.num(l, "ambiente"), obj = Diagnostico.num(estado, "setpoint");
        if (cam != null) {
            historia.addLast(new double[]{cam, obj == null ? Double.NaN : obj});
            while (historia.size() > 180) historia.removeFirst();
        }
        refrescar();
    }
    public void setEstado(Map<String, Object> e) { estado = e; refrescar(); }
    public void setConfig(Map<String, Object> c) { config = c; refrescar(); }
    public void setDispositivo(Map<String, Object> d) { dispositivo = d; refrescar(); }
    public void setEventos(List<Map<String, Object>> e) { eventos = e; refrescar(); }
    public void setVivo(boolean v) { if (v != vivo) { vivo = v; refrescar(); } }
    public void setConectado(boolean c) { conectado = c; refrescar(); }
    public void setControl(Control c) { control = c; refrescar(); }
    public void setUltimoComando(String t) { ultimoComando = t; refrescar(); }

    // ------------------------------------------------------------ construcción

    private int dp(float v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, act.getResources().getDisplayMetrics()));
    }

    private void reconstruir() {
        contenedor.removeAllViews();
        cuerpos.clear();
        for (Ventana v : acomodo) {
            MaterialCardView tarjeta = new MaterialCardView(act);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.topMargin = dp(10);
            tarjeta.setLayoutParams(lp);
            tarjeta.setRadius(dp(12));
            tarjeta.setStrokeWidth(dp(1));
            tarjeta.setStrokeColor(0xFF30363D);
            tarjeta.setCardBackgroundColor(0xFF161B22);

            LinearLayout dentro = new LinearLayout(act);
            dentro.setOrientation(LinearLayout.VERTICAL);

            // Barra de título como la del panel (sin botones: aquí solo se ve)
            LinearLayout barra = new LinearLayout(act);
            barra.setOrientation(LinearLayout.HORIZONTAL);
            barra.setGravity(Gravity.CENTER_VERTICAL);
            barra.setBackgroundColor(0xFF1F252E);
            barra.setPadding(dp(12), dp(8), dp(12), dp(8));
            TextView titulo = texto(TITULOS.get(v.id), 14, 0xFFE6EDF3, true);
            titulo.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            barra.addView(titulo);
            barra.addView(texto(v.min ? "minimizada" : v.id.equals("control") ? "control" : "solo lectura", 11,
                    v.id.equals("control") && !v.min ? 0xFF58A6FF : 0xFF8B949E, false));
            dentro.addView(barra);

            if (!v.min) {
                LinearLayout cuerpo = new LinearLayout(act);
                cuerpo.setOrientation(LinearLayout.VERTICAL);
                cuerpo.setPadding(dp(14), dp(10), dp(14), dp(12));
                dentro.addView(cuerpo);
                cuerpos.put(v.id, cuerpo);
                if (v.id.equals("gabinete")) {
                    if (gabinete == null) {
                        gabinete = new GabineteView(act);
                        gabinete.alTocarPieza(this::explicarPieza);
                    }
                    soltar(gabinete);
                    cuerpo.addView(gabinete);
                } else if (v.id.equals("grafica")) {
                    if (grafica == null) grafica = new Grafica(act);
                    soltar(grafica);
                    grafica.setLayoutParams(new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(160)));
                    cuerpo.addView(grafica);
                }
            }
            tarjeta.addView(dentro);
            contenedor.addView(tarjeta);
        }
        refrescar();
    }

    private static void soltar(View v) {
        if (v.getParent() instanceof ViewGroup) ((ViewGroup) v.getParent()).removeView(v);
    }

    /** Toque en una pieza del gabinete: qué es, cómo está y qué revisar. */
    private void explicarPieza(String id) {
        for (Diagnostico.Componente c : diag) {
            if (!c.id.equals(id)) continue;
            String msj = c.valor + "\n\n" + c.detalle + (c.queHacer.isEmpty() ? "" : "\n\nQué hacer: " + c.queHacer);
            new AlertDialog.Builder(act).setTitle(c.nombre).setMessage(msj).setPositiveButton("Entendido", null).show();
            return;
        }
    }

    // ------------------------------------------------------------ contenido de cada ventana

    private void refrescar() {
        diag = Diagnostico.diagnosticar(lecturas, estado, dispositivo, vivo, conectado);
        for (Map.Entry<String, LinearLayout> e : cuerpos.entrySet()) {
            LinearLayout c = e.getValue();
            switch (e.getKey()) {
                case "gabinete": gabinete.setDatos(lecturas, estado, vivo, diag); break;
                case "grafica": grafica.setDatos(historia); break;
                default: c.removeAllViews(); llenar(e.getKey(), c);
            }
        }
    }

    private void llenar(String id, LinearLayout c) {
        Map<String, Object> l = lecturas, s = estado, d = dispositivo;
        Map<String, Object> aj = mapa(s.get("ajustes"));
        switch (id) {
            case "resumen": {
                Double n = Diagnostico.num(s, "nivel");
                c.addView(texto(n == null ? "--" : "N" + n.intValue(), 34, Colores.argbNivel(n == null ? 0 : n.intValue()), true));
                fila(c, "Motivo", Diagnostico.txt(s, "motivo"));
                fila(c, "Modo", Diagnostico.txt(s, "modo"));
                fila(c, "Celdas", (Diagnostico.si(s, "celda_a") ? "A " : "") + (Diagnostico.si(s, "celda_b") ? "B" : "") + (!Diagnostico.si(s, "celda_a") && !Diagnostico.si(s, "celda_b") ? "apagadas" : ""));
                List<String> fallas = new ArrayList<>();
                int avisos = 0;
                for (Diagnostico.Componente x : diag) {
                    if (Diagnostico.FALLA.equals(x.estado)) fallas.add(x.nombre);
                    if (Diagnostico.AVISO.equals(x.estado)) avisos++;
                }
                String salud = !vivo ? "Sin datos actuales del ESP32" : !fallas.isEmpty() ? fallas.size() + " falla(s): " + android.text.TextUtils.join(", ", fallas)
                        : avisos > 0 ? avisos + " cosa(s) por revisar" : "Todo en orden";
                c.addView(texto(salud, 14, !vivo ? 0xFF8B949E : !fallas.isEmpty() ? 0xFFF85149 : avisos > 0 ? 0xFFD29922 : 0xFF3FB950, true));
                break;
            }
            case "diagnostico": {
                List<Diagnostico.Componente> orden = new ArrayList<>(diag);
                Collections.sort(orden, (a, b) -> Diagnostico.gravedad(b.estado) - Diagnostico.gravedad(a.estado));
                for (Diagnostico.Componente x : orden) {
                    c.addView(texto("● " + x.nombre + " · " + x.valor + "   " + etiqueta(x.estado), 14, colorEstado(x.estado), true));
                    c.addView(texto(x.detalle, 12, 0xFF8B949E, false));
                    if (!x.queHacer.isEmpty()) c.addView(texto("Qué hacer: " + x.queHacer, 12, 0xFFE6EDF3, false));
                }
                break;
            }
            case "control":
                fila(c, "Objetivo", Diagnostico.f1(Diagnostico.num(s, "setpoint"), " °C"));
                fila(c, "Modo", Diagnostico.txt(s, "modo"));
                fila(c, "Celda A en manual", Diagnostico.si(config, "manual_a") ? "ON" : "OFF");
                fila(c, "Celda B en manual", Diagnostico.si(config, "manual_b") ? "ON" : "OFF");
                fila(c, "Aviso de carga alta", Diagnostico.si(s, "carga_alta") ? "activado" : "no");
                fila(c, "Último cambio desde", "app".equals(Diagnostico.txt(config, "origen")) ? "la app" : "web".equals(Diagnostico.txt(config, "origen")) ? "el panel web" : "--");
                if (control != null) controles(c, s);
                break;
            case "temperaturas":
                fila(c, "Cámara", Diagnostico.f1(Diagnostico.num(l, "ambiente"), " °C"));
                fila(c, "Objetivo", Diagnostico.f1(Diagnostico.num(s, "setpoint"), " °C"));
                fila(c, "Tendencia", Diagnostico.num(l, "pendiente") == null ? "--" : String.format(Locale.getDefault(), "%+.2f °C/min", Diagnostico.num(l, "pendiente")));
                fila(c, "Prevista en 2 min", Diagnostico.f1(Diagnostico.num(l, "prediccion"), " °C"));
                fila(c, "Humedad", Diagnostico.f1(Diagnostico.num(l, "humedad"), " %"));
                fila(c, "Punto de rocío", Diagnostico.f1(Diagnostico.num(l, "punto_rocio"), " °C"));
                fila(c, "Bloque frío", Diagnostico.f1(Diagnostico.num(l, "placa_fria"), " °C"));
                fila(c, "Condensación", Diagnostico.si(s, "condensa") ? "Sí: bajo el rocío" : "No");
                break;
            case "ventilacion":
                fila(c, "PWM actual", Diagnostico.num(s, "pwm_fans") == null ? "--" : Math.round(Diagnostico.num(s, "pwm_fans")) + " %");
                fila(c, "Frecuencia", entero(Diagnostico.num(s, "pwm_hz"), " Hz"));
                fila(c, "Control", Boolean.FALSE.equals(s.get("pwm_auto")) ? "manual a " + entero(Diagnostico.num(s, "pwm_pct_manual"), " %") : "por nivel (auto)");
                fila(c, "Nivel 1 / 2 / 3", entero(Diagnostico.num(aj, "pwm_n1"), "") + " / " + entero(Diagnostico.num(aj, "pwm_n2"), "") + " / " + entero(Diagnostico.num(aj, "pwm_n3"), " %"));
                nota(c, "Bomba y 3 ventiladores comparten un MOSFET: se mueven juntos. Sin tacómetro: el % es la orden, no RPM.");
                break;
            case "fugas":
                fila(c, "Humedad junto a la bomba", entero(Diagnostico.num(l, "fuga_pct"), " %"));
                fila(c, "Estado", Diagnostico.si(s, "fuga") ? "FUGA: todo cortado" : "Seco");
                fila(c, "Lectura ADC", entero(Diagnostico.num(l, "fuga_raw"), ""));
                fila(c, "Umbral de fuga", entero(Diagnostico.num(aj, "fuga_umbral"), " %"));
                break;
            case "celdas":
                fila(c, "Celda A", (Diagnostico.si(s, "celda_a") ? "ON" : "OFF") + " · " + Diagnostico.f1(Diagnostico.num(s, "horas_a"), " h"));
                fila(c, "Celda B", (Diagnostico.si(s, "celda_b") ? "ON" : "OFF") + " · " + Diagnostico.f1(Diagnostico.num(s, "horas_b"), " h"));
                nota(c, "En nivel 2 trabaja la celda con menos horas.");
                break;
            case "protecciones":
                fila(c, "Aviso de congelamiento", Diagnostico.f1(Diagnostico.num(aj, "congela_aviso"), " °C"));
                fila(c, "Corte por congelamiento", Diagnostico.f1(Diagnostico.num(aj, "congela_corte"), " °C"));
                fila(c, "Rearme", Diagnostico.f1(Diagnostico.num(aj, "congela_rearme"), " °C"));
                fila(c, "Celda encendida mínimo", entero(Diagnostico.num(aj, "t_min_on_s"), " s"));
                fila(c, "Celda apagada mínimo", entero(Diagnostico.num(aj, "t_min_off_s"), " s"));
                fila(c, "Confirmación", entero(Diagnostico.num(aj, "t_confirma_s"), " s"));
                fila(c, "Pre / post circulación", entero(Diagnostico.num(aj, "t_precirc_s"), "") + " / " + entero(Diagnostico.num(aj, "t_postcirc_s"), " s"));
                break;
            case "pruebas": {
                String p = Diagnostico.txt(s, "prueba"), r = Diagnostico.txt(s, "prueba_resultado");
                fila(c, "Prueba en curso", p == null || p.isEmpty() ? "ninguna" : p);
                if (r != null && !r.isEmpty()) nota(c, r);
                nota(c, "Las pruebas las lanza el administrador desde el panel.");
                break;
            }
            case "registro": {
                if (eventos.isEmpty()) nota(c, "Todavía no hay eventos.");
                for (Map<String, Object> ev : eventos) {
                    Double ts = Diagnostico.num(ev, "ts");
                    String tipo = Diagnostico.txt(ev, "tipo");
                    c.addView(texto(Diagnostico.txt(ev, "texto"), 14, "alerta".equals(tipo) ? 0xFFF85149 : "sistema".equals(tipo) ? 0xFF58A6FF : 0xFFE6EDF3, false));
                    c.addView(texto((ts == null ? "--" : hora.format(new Date(ts.longValue()))) + " · N" + entero(Diagnostico.num(ev, "nivel"), ""), 11, 0xFF8B949E, false));
                }
                break;
            }
            case "dispositivo": {
                Double up = Diagnostico.num(s, "uptime_s");
                fila(c, "Firmware", Diagnostico.txt(d, "firmware"));
                fila(c, "Encendido hace", up == null ? "--" : String.format(Locale.getDefault(), "%d h %d min", up.longValue() / 3600, (up.longValue() % 3600) / 60));
                fila(c, "Último reinicio", Diagnostico.txt(d, "ultimo_reinicio"));
                fila(c, "IP", Diagnostico.txt(d, "ip"));
                fila(c, "Red", Diagnostico.txt(d, "ssid") + " · canal " + entero(Diagnostico.num(d, "canal"), ""));
                fila(c, "Señal", entero(Diagnostico.num(d, "rssi"), " dBm"));
                fila(c, "Caídas de Wi-Fi", entero(Diagnostico.num(d, "caidas_wifi"), ""));
                fila(c, "Temp. del chip", Diagnostico.f1(Diagnostico.num(d, "temp_chip"), " °C"));
                fila(c, "Memoria libre", entero(Diagnostico.num(d, "heap_libre_kb"), " KB"));
                break;
            }
            case "latencia":
                fila(c, "Última orden desde la app", ultimoComando == null ? "ninguna todavía" : ultimoComando);
                nota(c, "Ida y vuelta: la app escribe en config/ y espera el ack/ del ESP32 (meta < 500 ms).");
                fila(c, "IP del ESP32", Diagnostico.txt(d, "ip"));
                break;
            case "sistema":
                fila(c, "Buzzer", Diagnostico.si(s, "silencio") ? "silenciado" : "activo");
                fila(c, "Calibración cámara", Diagnostico.f1(Diagnostico.num(aj, "cal_camara"), " °C"));
                fila(c, "Calibración humedad", Diagnostico.f1(Diagnostico.num(aj, "cal_humedad"), " %"));
                fila(c, "Calibración bloque", Diagnostico.f1(Diagnostico.num(aj, "cal_bloque"), " °C"));
                break;
            case "sin_sensor":
                for (String[] x : Arrays.asList(
                        new String[]{"rpm_ventiladores", "RPM de los ventiladores", "rpm"},
                        new String[]{"voltaje_12v", "Voltaje de 12 V", "V"},
                        new String[]{"corriente_celdas", "Corriente de las celdas", "A"},
                        new String[]{"voltaje_5v", "Voltaje de 5 V", "V"},
                        new String[]{"agua", "Temperatura del agua", "°C"})) {
                    Double v = Diagnostico.num(l, x[0]);
                    fila(c, x[1], v == null ? "SIN SENSOR" : String.format(Locale.getDefault(), "%.1f %s", v, x[2]));
                }
                break;
            default:
                break;
        }
    }

    // ------------------------------------------------------------ utilidades

    @SuppressWarnings("unchecked")
    private static Map<String, Object> mapa(Object o) {
        return o instanceof Map ? (Map<String, Object>) o : Collections.emptyMap();
    }

    private static String entero(Double v, String u) { return v == null ? "--" : Math.round(v) + u; }

    private static String etiqueta(String est) {
        switch (est) {
            case Diagnostico.FALLA: return "FALLA";
            case Diagnostico.AVISO: return "REVISAR";
            case Diagnostico.ACTIVO: return "ACTIVO";
            case Diagnostico.OK: return "OK";
            case Diagnostico.SIN_SENSOR: return "NO INSTALADO";
            default: return "SIN DATO";
        }
    }

    private static int colorEstado(String est) {
        switch (est) {
            case Diagnostico.FALLA: return 0xFFF85149;
            case Diagnostico.AVISO: return 0xFFD29922;
            case Diagnostico.ACTIVO: return 0xFF39C5CF;
            case Diagnostico.OK: return 0xFF3FB950;
            default: return 0xFF8B949E;
        }
    }

    private TextView texto(String t, float sp, int color, boolean negrita) {
        TextView v = new TextView(act);
        v.setText(t == null ? "--" : t);
        v.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        v.setTextColor(color);
        if (negrita) v.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        v.setPadding(0, dp(2), 0, dp(2));
        return v;
    }

    private void fila(LinearLayout c, String k, String v) {
        LinearLayout f = new LinearLayout(act);
        f.setOrientation(LinearLayout.HORIZONTAL);
        TextView a = texto(k, 13, 0xFF8B949E, false);
        a.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        TextView b = texto(v == null ? "--" : v, 13, 0xFFE6EDF3, true);
        b.setGravity(Gravity.END);
        f.addView(a);
        f.addView(b);
        c.addView(f);
    }

    private void nota(LinearLayout c, String t) { c.addView(texto(t, 12, 0xFF8B949E, false)); }

    /** Botones del objetivo (−/+ 0.5 °C) y del modo. Sin conexión se desactivan: la orden no llegaría. */
    private void controles(LinearLayout c, Map<String, Object> s) {
        boolean puede = !Boolean.FALSE.equals(conectado);
        String modo = Diagnostico.txt(s, "modo");
        LinearLayout objetivo = filaBotones(c);
        boton(objetivo, "− 0.5 °C", puede, "Bajar el objetivo medio grado", () -> control.cambiarObjetivo(-Contrato.PASO_SETPOINT));
        boton(objetivo, "+ 0.5 °C", puede, "Subir el objetivo medio grado", () -> control.cambiarObjetivo(Contrato.PASO_SETPOINT));
        LinearLayout modos = filaBotones(c);
        boton(modos, Contrato.MODO_AUTO.equals(modo) ? "● Auto" : "Auto", puede, "Modo automático", () -> control.cambiarModo(Contrato.MODO_AUTO));
        boton(modos, Contrato.MODO_OFF.equals(modo) ? "● Apagado" : "Apagado", puede, "Apagar el enfriamiento", () -> control.cambiarModo(Contrato.MODO_OFF));
        nota(c, puede ? "Las órdenes llegan al ESP32 igual que desde el panel web; el control manual de celdas es del administrador."
                : "Sin internet: los botones vuelven al recuperar la conexión.");
    }

    private LinearLayout filaBotones(LinearLayout c) {
        LinearLayout f = new LinearLayout(act);
        f.setOrientation(LinearLayout.HORIZONTAL);
        f.setPadding(0, dp(6), 0, 0);
        c.addView(f);
        return f;
    }

    private void boton(LinearLayout fila, String texto, boolean activo, String descripcion, Runnable alTocar) {
        MaterialButton b = new MaterialButton(act);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1);
        lp.setMarginEnd(dp(6));
        b.setLayoutParams(lp);
        b.setText(texto);
        b.setAllCaps(false);
        b.setEnabled(activo);
        b.setContentDescription(descripcion);
        b.setOnClickListener(v -> alTocar.run());
        fila.addView(b);
    }

    /** Gráfica simple de la cámara (azul-naranja) y el objetivo (gris), con lo recibido desde que se abrió la app. */
    static final class Grafica extends View {
        private final Paint linea = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint letra = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final List<double[]> puntos = new ArrayList<>();

        Grafica(Context c) {
            super(c);
            linea.setStyle(Paint.Style.STROKE);
            letra.setColor(0xFF8B949E);
            letra.setTextSize(28);
            setContentDescription("Gráfica de la temperatura de la cámara");
        }

        void setDatos(ArrayDeque<double[]> h) {
            puntos.clear();
            puntos.addAll(h);
            invalidate();
        }

        @Override protected void onDraw(@NonNull Canvas c) {
            super.onDraw(c);
            if (puntos.size() < 2) { c.drawText("Esperando lecturas…", 10, getHeight() / 2f, letra); return; }
            double min = Double.MAX_VALUE, max = -Double.MAX_VALUE;
            for (double[] p : puntos) for (double v : p) if (!Double.isNaN(v)) { min = Math.min(min, v); max = Math.max(max, v); }
            if (max - min < 1) { max += 0.5; min -= 0.5; }
            float w = getWidth(), h = getHeight() - 10;
            for (int serie = 1; serie >= 0; serie--) {
                Path p = new Path();
                boolean inicio = true;
                for (int i = 0; i < puntos.size(); i++) {
                    double v = puntos.get(i)[serie];
                    if (Double.isNaN(v)) continue;
                    float x = w * i / (puntos.size() - 1), y = (float) (h - (v - min) / (max - min) * (h - 10)) + 5;
                    if (inicio) { p.moveTo(x, y); inicio = false; } else p.lineTo(x, y);
                }
                linea.setStrokeWidth(serie == 0 ? 5 : 3);
                linea.setColor(serie == 0 ? 0xFFD95926 : 0xFF8B949E);
                c.drawPath(p, linea);
            }
            c.drawText(String.format(Locale.getDefault(), "%.1f °C", max), 6, 30, letra);
            c.drawText(String.format(Locale.getDefault(), "%.1f °C", min), 6, getHeight() - 6, letra);
        }
    }
}
