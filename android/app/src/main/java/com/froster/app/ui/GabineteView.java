package com.froster.app.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PathMeasure;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.froster.app.datos.Diagnostico;

import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * El mismo gabinete animado del panel web (web/src/widgets/Gabinete.jsx), dibujado con Canvas.
 * Se mueve con los datos reales del ESP32: el aire va más rápido con más PWM, el agua solo circula
 * con la bomba encendida, el calor solo sale con alguna celda encendida, y cada pieza toma el color
 * de su diagnóstico. Al tocar una pieza avisa a quien escucha (la pantalla muestra qué revisar).
 */
public class GabineteView extends View {

    public interface AlTocarPieza { void con(String id); }

    private static final float ANCHO = 720f, ALTO = 420f;     // mismo viewBox que el SVG del panel

    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint texto = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Map<String, RectF> zonas = new LinkedHashMap<>();
    private final Map<String, String> estados = new HashMap<>();
    private final Path aireFrio = new Path(), aireRegreso = new Path(), aireRegreso2 = new Path(), calor = new Path(),
            aguaIda = new Path(), aguaVuelta = new Path(), aguaCubeta = new Path();
    private final PathMeasure medida = new PathMeasure();
    private AlTocarPieza alTocar;

    // Lo que publica el ESP32
    private int pwm = 0, celdas = 0, nivel = 0;
    private boolean vivo, bomba, fuga, sinHielo, condensa, celdaA, celdaB, alerta;
    private Double camara, setpoint, bloque;
    private float angulo = 0;
    private long ultimoCuadro = 0;

    public GabineteView(Context c) { this(c, null); }
    public GabineteView(Context c, @Nullable AttributeSet a) {
        super(c, a);
        texto.setColor(0xFFC9D1D9);
        texto.setTextSize(11f);
        aireFrio.moveTo(400, 150); aireFrio.cubicTo(330, 150, 250, 170, 150, 175); aireFrio.cubicTo(110, 178, 95, 150, 100, 120);
        aireRegreso.moveTo(100, 120); aireRegreso.cubicTo(110, 70, 200, 62, 300, 66); aireRegreso.cubicTo(360, 68, 395, 90, 400, 110);
        aireRegreso2.moveTo(140, 220); aireRegreso2.cubicTo(220, 238, 330, 236, 400, 195);
        calor.moveTo(515, 150); calor.cubicTo(560, 150, 600, 140, 700, 125);
        aguaIda.moveTo(425, 318); aguaIda.lineTo(425, 250); aguaIda.lineTo(493, 250); aguaIda.lineTo(493, 228);
        aguaVuelta.moveTo(493, 72); aguaVuelta.lineTo(493, 58); aguaVuelta.lineTo(625, 58); aguaVuelta.lineTo(625, 300); aguaVuelta.lineTo(590, 300);
        aguaCubeta.moveTo(590, 330); aguaCubeta.lineTo(445, 330);
        zonas.put("dht22", new RectF(90, 55, 125, 105));
        zonas.put("ventiladores", new RectF(360, 88, 405, 218));
        zonas.put("ds18b20", new RectF(405, 70, 449, 230));
        zonas.put("celda_a", new RectF(449, 80, 469, 146));
        zonas.put("celda_b", new RectF(449, 154, 469, 220));
        zonas.put("agua", new RectF(469, 72, 517, 228));
        zonas.put("bomba", new RectF(405, 310, 445, 350));
        zonas.put("fuga", new RectF(398, 352, 450, 372));
        zonas.put("esp32", new RectF(94, 296, 172, 348));
        zonas.put("wifi", new RectF(145, 272, 175, 298));
        zonas.put("buzzer", new RectF(298, 300, 322, 324));
        setContentDescription("Gabinete de FROSTER");
    }

    public void alTocarPieza(AlTocarPieza l) { alTocar = l; }

    /** Datos nuevos de Firebase + diagnóstico ya calculado. */
    public void setDatos(Map<String, Object> l, Map<String, Object> e, boolean vivo, Iterable<Diagnostico.Componente> diag) {
        this.vivo = vivo;
        bomba = Diagnostico.si(e, "bomba");
        fuga = Diagnostico.si(e, "fuga");
        sinHielo = Diagnostico.si(e, "sin_hielo");
        condensa = Diagnostico.si(e, "condensa");
        celdaA = Diagnostico.si(e, "celda_a");
        celdaB = Diagnostico.si(e, "celda_b");
        celdas = (celdaA ? 1 : 0) + (celdaB ? 1 : 0);
        Double pw = Diagnostico.num(e, "pwm_fans"), n = Diagnostico.num(e, "nivel"), a = Diagnostico.num(e, "alerta_id");
        boolean probando = "bomba".equals(Diagnostico.txt(e, "prueba"));
        pwm = probando ? 100 : bomba && pw != null ? pw.intValue() : 0;
        nivel = n == null ? 0 : n.intValue();
        alerta = a != null && a > 0;
        camara = Diagnostico.num(l, "ambiente");
        bloque = Diagnostico.num(l, "placa_fria");
        setpoint = Diagnostico.num(e, "setpoint");
        estados.clear();
        int fallas = 0;
        for (Diagnostico.Componente c : diag) {
            estados.put(c.id, c.estado);
            if (Diagnostico.FALLA.equals(c.estado)) fallas++;
        }
        setContentDescription("Gabinete: " + fallas + " fallas, ventiladores al " + pwm + " %, " + celdas + " celdas encendidas");
        invalidate();
    }

    @Override
    protected void onMeasure(int w, int h) {
        int ancho = MeasureSpec.getSize(w);
        setMeasuredDimension(ancho, Math.round(ancho * ALTO / ANCHO));
    }

    private boolean mueve() { return vivo && pwm > 0; }

    private static int color(String estado) {
        if (estado == null) return 0xFF6E7681;
        switch (estado) {
            case Diagnostico.FALLA: return 0xFFF85149;
            case Diagnostico.AVISO: return 0xFFD29922;
            case Diagnostico.ACTIVO: return 0xFF39C5CF;
            case Diagnostico.OK: return 0xFF3FB950;
            case Diagnostico.SIN_SENSOR: return 0xFF484F58;
            default: return 0xFF6E7681;
        }
    }

    private int colorRegreso() {
        if (camara == null || setpoint == null) return 0xFF8B949E;
        double d = camara - setpoint;
        if (d <= -0.5) return 0xFF58A6FF;
        if (d <= 1) return 0xFF39C5CF;
        if (d <= 2.5) return 0xFFD29922;
        return 0xFFF85149;
    }

    @Override
    protected void onDraw(@NonNull Canvas c) {
        super.onDraw(c);
        long ahora = SystemClock.uptimeMillis();
        float dt = ultimoCuadro == 0 ? 0 : (ahora - ultimoCuadro) / 1000f;
        ultimoCuadro = ahora;
        float k = getWidth() / ANCHO;
        c.save();
        c.scale(k, k);
        boolean parpadeo = (ahora / 500) % 2 == 0;

        // Caja, cámara y bahía de electrónica
        rect(c, 60, 30, 660, 395, 12, 0xFF0F141A, 0xFF3B434D, 3);
        p.setPathEffect(new DashPathEffect(new float[]{6, 4}, 0));
        rect(c, 80, 45, 400, 250, 8, 0x0D58A6FF, 0xFF2D4A6B, 1.5f);
        p.setPathEffect(null);
        rect(c, 80, 270, 380, 380, 8, 0xFF12171E, 0xFF30363D, 1);
        texto.setColor(0xFF8B949E); texto.setFakeBoldText(true);
        c.drawText("CÁMARA · " + Diagnostico.f1(camara, " °C") + " · objetivo " + Diagnostico.f1(setpoint, " °C"), 92, 240, texto);
        c.drawText("ELECTRÓNICA", 90, 288, texto);
        texto.setFakeBoldText(false); texto.setColor(0xFFC9D1D9);

        // Circuito de agua
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(6); p.setStrokeCap(Paint.Cap.ROUND);
        p.setColor(0xFF2D4A6B); c.drawPath(aguaIda, p);
        p.setColor(celdas > 0 ? 0xFF6E3630 : 0xFF2D4A6B); c.drawPath(aguaVuelta, p);

        // Cubeta de hielo
        rect(c, 445, 290, 605, 375, 6, sinHielo ? 0xFF2B1D0E : 0xFF0D2238, sinHielo ? 0xFFD29922 : 0xFF3987E5, 1.5f);
        if (!sinHielo) for (int i = 0; i < 5; i++) rect(c, 460 + i * 27, 305 + (i % 2) * 18, 478 + i * 27, 321 + (i % 2) * 18, 3, 0xD9CAE8FF, 0, 0);
        c.drawText(sinHielo ? "SIN HIELO" : "cubeta de hielo", 455, 368, texto);

        // Piezas con su color de diagnóstico
        pieza(c, "dht22", 96, 60, 118, 90, 3, 0xFF1C2128, parpadeo);
        c.drawText("DHT22", 96, 102, texto);
        rect(c, 405, 70, 449, 230, 4, 0xE63987E5, color(estados.get("ds18b20")), 2);
        for (int x = 408; x < 449; x += 6) { p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(1); p.setColor(0x660D1117); c.drawLine(x, 72, x, 228, p); }
        c.drawText(Diagnostico.f1(bloque, " °C"), 405, 245, texto);
        if (condensa) { p.setStyle(Paint.Style.FILL); p.setColor(0xFF58A6FF); for (int i = 0; i < 3; i++) c.drawCircle(412 + i * 14, 236 + (ahora / 120 + i * 3) % 8, 2.5f, p); }
        pieza(c, "celda_a", 451, 82, 467, 144, 2, celdaA ? 0xFF1F6FEB : 0xFF2A2F37, parpadeo);
        pieza(c, "celda_b", 451, 156, 467, 218, 2, celdaB ? 0xFF1F6FEB : 0xFF2A2F37, parpadeo);
        c.drawText("A", 455, 118, texto); c.drawText("B", 455, 192, texto);
        pieza(c, "agua", 469, 72, 517, 228, 4, 0xFF1B2A3A, parpadeo);
        c.drawText("agua", 478, 155, texto);
        pieza(c, "fuga", 405, 356, 445, 366, 2, 0xFF1C2128, parpadeo);
        c.drawText("sensor fuga", 400, 384, texto);
        // Bomba
        p.setStyle(Paint.Style.FILL); p.setColor(mueve() && !fuga ? 0xFF0D419D : 0xFF1C2128); c.drawCircle(425, 330, 17, p);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(2); p.setColor(color(estados.get("bomba")));
        if (!(Diagnostico.FALLA.equals(estados.get("bomba")) && !parpadeo)) c.drawCircle(425, 330, 17, p);
        c.drawText("P", 421, 334, texto);
        if (fuga) { p.setStyle(Paint.Style.FILL); p.setColor(0xFF79C0FF); for (int i = 0; i < 4; i++) c.drawCircle(410 + i * 10, 370 + (ahora / 90 + i * 4) % 10, 3, p); }

        // Ventiladores girando según el PWM
        if (mueve()) angulo = (angulo + dt * 360f * (pwm / 100f) * 2.2f) % 360f;
        int cv = color(estados.get("ventiladores"));
        ventilador(c, 382, 110, cv, parpadeo); ventilador(c, 382, 195, cv, parpadeo); ventilador(c, 640, 150, cv, parpadeo);
        c.drawText("extrae", 624, 186, texto);

        // Electrónica
        pieza(c, "esp32", 94, 296, 172, 348, 4, 0xFF0B3D2E, parpadeo);
        c.drawText("ESP32", 114, 326, texto);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(2.5f); p.setColor(color(estados.get("wifi")));
        c.drawLine(160, 296, 160, 286, p);
        c.drawArc(new RectF(148, 276, 172, 300), 225, 90, false, p);
        c.drawArc(new RectF(141, 268, 179, 306), 225, 90, false, p);
        String[] m = {"A", "B", "P"};
        boolean[] on = {celdaA, celdaB, mueve()};
        for (int i = 0; i < 3; i++) {
            rect(c, 190 + i * 32, 300, 214 + i * 32, 334, 2, on[i] ? 0xFF1A7F37 : 0xFF21262D, on[i] ? 0xFF3FB950 : 0xFF484F58, 1);
            c.drawText(m[i], 198 + i * 32, 346, texto);
        }
        c.drawText("MOSFET D4184", 188, 362, texto);
        pieza(c, "buzzer", 300, 302, 320, 322, 10, 0xFF1C2128, parpadeo);
        c.drawText("buzzer", 296, 338, texto);
        p.setStyle(Paint.Style.FILL);
        p.setColor(alerta ? (parpadeo ? 0xFFF85149 : 0xFF3A1414) : nivel == 3 ? 0xFF58A6FF : nivel == 2 ? 0xFF39C5CF : 0xFF3FB950);
        c.drawCircle(352, 312, 7, p);
        c.drawText("LED", 343, 338, texto);

        // Aire y agua en movimiento
        if (mueve()) {
            float vel = Math.max(pwm, 15) / 100f;
            float durAire = 6f / vel, durAgua = 5f / vel;
            float t = ahora / 1000f;
            if (celdas > 0) particulas(c, calor, 6, durAire * 0.6f, t, 0xFFF85149, 3.2f);
            particulas(c, aireFrio, 9, durAire, t, 0xFF58A6FF, 3.2f);
            particulas(c, aireRegreso, 8, durAire, t, colorRegreso(), 3.2f);
            particulas(c, aireRegreso2, 6, durAire * 0.8f, t, colorRegreso(), 3.2f);
            if (!fuga) {
                particulas(c, aguaIda, 5, durAgua * 0.5f, t, 0xFF58A6FF, 2.6f);
                particulas(c, aguaVuelta, 8, durAgua, t, celdas > 0 ? 0xFFFF7B72 : 0xFF58A6FF, 2.6f);
                particulas(c, aguaCubeta, 4, durAgua * 0.5f, t, 0xFF58A6FF, 2.6f);
            }
        } else {
            texto.setColor(0xFF6E7681);
            c.drawText(vivo ? "Ventilación apagada" : "Sin datos del ESP32", 170, 150, texto);
            texto.setColor(0xFFC9D1D9);
        }

        // Globos en las piezas con falla o aviso
        globo(c, "bomba", "Bomba", 378, 292); globo(c, "fuga", "Fuga", 452, 390); globo(c, "dht22", "DHT22", 120, 92);
        globo(c, "ds18b20", "Bloque frío", 410, 262); globo(c, "ventiladores", "Ventiladores", 300, 268);
        globo(c, "esp32", "ESP32", 94, 370); globo(c, "wifi", "Wi-Fi", 184, 262); globo(c, "celda_a", "Celda A", 520, 95);
        globo(c, "celda_b", "Celda B", 520, 215); globo(c, "buzzer", "Buzzer", 290, 290);
        c.restore();

        // Solo se redibuja seguido si hay algo que se mueve o parpadea
        if (mueve() || estados.containsValue(Diagnostico.FALLA) || alerta || condensa || fuga) postInvalidateOnAnimation();
        else ultimoCuadro = 0;
    }

    private void particulas(Canvas c, Path camino, int n, float dur, float t, int color, float r) {
        medida.setPath(camino, false);
        float largo = medida.getLength();
        float[] pos = new float[2];
        p.setStyle(Paint.Style.FILL);
        p.setColor(color);
        for (int i = 0; i < n; i++) {
            float fase = ((t / dur) + (float) i / n) % 1f;
            medida.getPosTan(largo * fase, pos, null);
            c.drawCircle(pos[0], pos[1], r, p);
        }
    }

    private void ventilador(Canvas c, float x, float y, int borde, boolean parpadeo) {
        p.setStyle(Paint.Style.FILL); p.setColor(0xFF161B22); c.drawCircle(x, y, 17, p);
        p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(2); p.setColor(borde);
        if (!(borde == 0xFFF85149 && !parpadeo)) c.drawCircle(x, y, 17, p);
        c.save(); c.translate(x, y); c.rotate(angulo);
        p.setStyle(Paint.Style.FILL); p.setColor(0xFF8B949E);
        for (int i = 0; i < 3; i++) {
            Path aspa = new Path();
            aspa.moveTo(0, 0); aspa.cubicTo(4, -6, 10, -12, 2, -15); aspa.cubicTo(-3, -10, -3, -5, 0, 0);
            c.drawPath(aspa, p); c.rotate(120);
        }
        c.restore();
        p.setColor(0xFFC9D1D9); c.drawCircle(x, y, 3, p);
    }

    private void pieza(Canvas c, String id, float l, float t, float r, float b, float radio, int relleno, boolean parpadeo) {
        String est = estados.get(id);
        boolean apagar = Diagnostico.FALLA.equals(est) && !parpadeo;
        rect(c, l, t, r, b, radio, relleno, apagar ? 0 : color(est), 2);
    }

    private void globo(Canvas c, String id, String nombre, float x, float y) {
        String est = estados.get(id);
        if (!Diagnostico.FALLA.equals(est) && !Diagnostico.AVISO.equals(est)) return;
        String t = "⚠ " + nombre;
        float w = texto.measureText(t) + 10;
        rect(c, x - 4, y - 13, x - 4 + w, y + 5, 4, color(est), 0, 0);
        int antes = texto.getColor();
        texto.setColor(0xFF0D1117); texto.setFakeBoldText(true);
        c.drawText(t, x + 1, y, texto);
        texto.setColor(antes); texto.setFakeBoldText(false);
    }

    private void rect(Canvas c, float l, float t, float r, float b, float radio, int relleno, int borde, float grosor) {
        RectF z = new RectF(l, t, r, b);
        p.setPathEffect(p.getPathEffect());
        if (Color.alpha(relleno) > 0) { p.setStyle(Paint.Style.FILL); p.setColor(relleno); c.drawRoundRect(z, radio, radio, p); }
        if (grosor > 0 && Color.alpha(borde) > 0) {
            p.setStyle(Paint.Style.STROKE); p.setStrokeWidth(grosor); p.setColor(borde); c.drawRoundRect(z, radio, radio, p);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN) return true;
        if (ev.getAction() != MotionEvent.ACTION_UP) return super.onTouchEvent(ev);
        float k = getWidth() / ANCHO;
        float x = ev.getX() / k, y = ev.getY() / k;
        for (Map.Entry<String, RectF> z : zonas.entrySet()) {
            RectF r = new RectF(z.getValue());
            r.inset(-6, -6);
            if (r.contains(x, y)) {
                if (alTocar != null) alTocar.con(z.getKey());
                performClick();
                return true;
            }
        }
        return true;
    }

    @Override
    public boolean performClick() { return super.performClick(); }
}
