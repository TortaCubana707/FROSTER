package com.froster.app.ui;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import com.froster.app.R;
import com.froster.app.datos.Contrato;
import com.froster.app.datos.Repositorio;
import com.google.android.material.card.MaterialCardView;
import com.google.android.material.snackbar.Snackbar;
import com.google.firebase.auth.FirebaseAuth;
import com.google.firebase.database.DataSnapshot;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Pantalla principal (nivel USUARIO): datos en vivo del ESP32, ajuste del objetivo y avisos.
 * El modo, las celdas en manual y el aviso de carga alta son del nivel ADMINISTRADOR (tablero web);
 * aquí solo se muestran, y las reglas de Firebase rechazan esas escrituras desde una cuenta de usuario.
 *
 * Avisos al usuario, de menor a mayor urgencia:
 *  - Toast: validaciones locales (objetivo en su límite) y errores de inicio de sesión.
 *  - Snackbar: confirmación de cada comando con su latencia, alerta resuelta, sin conexión.
 *  - Banner rojo + AlertDialog + notificación con sonido + alarma (tono y vibración): alerta nueva.
 */
public class MainActivity extends AppCompatActivity {

    private static final int EVENTOS_EN_PANTALLA = 10;
    private static final long ESPERA_SIN_CONEXION_MS = 3000;   // evita el aviso falso al abrir la app

    private Repositorio repo;
    private PanelEspejo espejo;          // mismas ventanas que el panel del administrador
    private View vistaClasica;
    private boolean verClasica = false;
    private final List<Repositorio.Suscripcion> suscripciones = new ArrayList<>();
    private final Handler reloj = new Handler(Looper.getMainLooper());
    private final DateFormat hora = DateFormat.getTimeInstance(DateFormat.MEDIUM);

    private View raiz;
    private MaterialCardView cardAlerta, cardNivel;
    private TextView txtLatido, txtConexion, txtAlerta, txtNivel, txtNivelDetalle, txtMotivo,
            indCeldaA, indCeldaB, indBomba, indVentiladores,
            txtCamara, txtHumedad, txtTendencia, txtBloque, txtRocio, txtFuga,
            txtObjetivo, txtLatencia, txtSinEventos, txtDispositivo, txtModo;
    private LinearLayout listaEventos;
    private AlertDialog dialogoAlerta;
    private Snackbar avisoSinConexion;
    private boolean enModoOffline = false;   // para avisar "Conexión recuperada" solo si hubo corte

    // Último estado conocido
    private long ultimoLatido = 0, uptimeAnterior = -1;
    private int alertaAnterior = Contrato.ALERTA_NINGUNA;
    private double setpoint = 28.0;
    private String modo = Contrato.MODO_AUTO;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        repo = Repositorio.de(this);

        raiz = findViewById(R.id.raiz);
        cardAlerta = findViewById(R.id.card_alerta);
        cardNivel = findViewById(R.id.card_nivel);
        txtLatido = findViewById(R.id.txt_latido);
        txtConexion = findViewById(R.id.txt_conexion);
        txtAlerta = findViewById(R.id.txt_alerta);
        txtNivel = findViewById(R.id.txt_nivel);
        txtNivelDetalle = findViewById(R.id.txt_nivel_detalle);
        txtMotivo = findViewById(R.id.txt_motivo);
        indCeldaA = findViewById(R.id.ind_celda_a);
        indCeldaB = findViewById(R.id.ind_celda_b);
        indBomba = findViewById(R.id.ind_bomba);
        indVentiladores = findViewById(R.id.ind_ventiladores);
        txtCamara = findViewById(R.id.txt_camara);
        txtHumedad = findViewById(R.id.txt_humedad);
        txtTendencia = findViewById(R.id.txt_tendencia);
        txtBloque = findViewById(R.id.txt_bloque);
        txtRocio = findViewById(R.id.txt_rocio);
        txtFuga = findViewById(R.id.txt_fuga);
        txtObjetivo = findViewById(R.id.txt_objetivo);
        txtLatencia = findViewById(R.id.txt_latencia);
        txtSinEventos = findViewById(R.id.txt_sin_eventos);
        txtDispositivo = findViewById(R.id.txt_dispositivo);
        listaEventos = findViewById(R.id.lista_eventos);
        txtModo = findViewById(R.id.txt_modo);
        vistaClasica = findViewById(R.id.vista_clasica);
        espejo = new PanelEspejo(this, findViewById(R.id.panel_espejo));

        // Control cruzado (rúbrica del Proyecto 1): el usuario cambia el objetivo y el modo desde la app
        // igual que desde el panel web; el ESP32 confirma cada orden en ack/.
        findViewById(R.id.btn_menos).setOnClickListener(v -> cambiarObjetivo(-Contrato.PASO_SETPOINT));
        findViewById(R.id.btn_mas).setOnClickListener(v -> cambiarObjetivo(Contrato.PASO_SETPOINT));
        espejo.setControl(new PanelEspejo.Control() {
            @Override public void cambiarObjetivo(double delta) { MainActivity.this.cambiarObjetivo(delta); }
            @Override public void cambiarModo(String m) { MainActivity.this.cambiarModo(m); }
        });
        findViewById(R.id.btn_historial).setOnClickListener(v -> abrirHistorial());

        if (Build.VERSION.SDK_INT >= 33 && ContextCompat.checkSelfPermission(this,
                Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 1);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        suscripciones.add(repo.escuchar(Contrato.LECTURAS, this::pintarLecturas));
        suscripciones.add(repo.escuchar(Contrato.ESTADO, this::pintarEstado));
        suscripciones.add(repo.escuchar(Contrato.DISPOSITIVO, this::pintarDispositivo));
        suscripciones.add(repo.escuchar(Contrato.CONECTADO, this::pintarConexion));
        suscripciones.add(repo.escucharUltimosEventos(EVENTOS_EN_PANTALLA, this::pintarEventos));
        suscripciones.add(repo.escuchar(Contrato.UI_LAYOUT, s -> espejo.setAcomodo(s.getValue())));
        suscripciones.add(repo.escuchar(Contrato.CONFIG, s -> espejo.setConfig(mapa(s))));
        reloj.post(revisarLatido);
        vigilarRed();
    }

    @Override
    protected void onStop() {
        super.onStop();
        for (Repositorio.Suscripcion s : suscripciones) s.cancelar();
        suscripciones.clear();
        reloj.removeCallbacksAndMessages(null);
        dejarDeVigilarRed();
    }

    // ------------------------------------------------------------ red del teléfono (modo offline)

    private ConnectivityManager red;
    private ConnectivityManager.NetworkCallback alCambiarRed;

    /** ¿El teléfono tiene una red con internet (Wi-Fi o datos)? */
    private boolean hayRed() {
        if (red == null) return true;
        NetworkCapabilities nc = red.getNetworkCapabilities(red.getActiveNetwork());
        return nc != null && nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
    }

    /**
     * Receptor dinámico de cambios de red (ConnectivityManager, registrado en onStart y quitado en onStop).
     * Al apagar el Wi-Fi o los datos avisa del modo offline de inmediato; .info/connected de Firebase
     * cubre el caso en que hay red pero no se llega a Firebase, y avisa la reconexión.
     */
    private void vigilarRed() {
        red = getSystemService(ConnectivityManager.class);
        if (red == null) return;
        alCambiarRed = new ConnectivityManager.NetworkCallback() {
            @Override public void onLost(Network n) {
                reloj.post(() -> {            // el callback llega en otro hilo: la UI se toca en el principal
                    if (!hayRed() && !enModoOffline) {
                        reloj.removeCallbacks(avisarSinConexion);
                        avisarSinConexion.run();
                    }
                });
            }
        };
        try {
            red.registerDefaultNetworkCallback(alCambiarRed);
        } catch (RuntimeException e) {
            alCambiarRed = null;              // sin permiso o servicio: queda .info/connected
        }
        if (!hayRed() && !enModoOffline) avisarSinConexion.run();   // se abrió la app sin internet
    }

    private void dejarDeVigilarRed() {
        if (red != null && alCambiarRed != null) {
            try { red.unregisterNetworkCallback(alCambiarRed); } catch (RuntimeException ignorada) { }
        }
        alCambiarRed = null;
    }

    @Override
    protected void onDestroy() {
        if (dialogoAlerta != null) dialogoAlerta.dismiss();
        Notificaciones.quitarModoOffline(this);   // no dejar el aviso fijo si la app se cierra
        super.onDestroy();
    }

    // ------------------------------------------------------------ lo que llega de Firebase

    private void pintarLecturas(DataSnapshot l) {
        txtCamara.setText(grados(numero(l, "ambiente")));
        Double humedad = numero(l, "humedad");
        txtHumedad.setText(humedad == null ? getString(R.string.humedad_sin_dato)
                : getString(R.string.humedad_pct, humedad));
        Double pendiente = numero(l, "pendiente"), prediccion = numero(l, "prediccion");
        txtTendencia.setText(pendiente == null || prediccion == null ? ""
                : getString(R.string.tendencia, pendiente, prediccion));
        txtBloque.setText(grados(numero(l, "placa_fria")));
        Double rocio = numero(l, "punto_rocio");
        txtRocio.setText(rocio == null ? "" : getString(R.string.rocio, rocio));
        Double fuga = numero(l, "fuga_pct");
        txtFuga.setText(fuga == null ? "" : getString(R.string.fuga_pct, fuga));
        repo.guardarLectura(l);
        espejo.setLecturas(mapa(l));
    }

    private void pintarEstado(DataSnapshot e) {
        Integer n = e.child("nivel").getValue(Integer.class);
        Integer a = e.child("alerta_id").getValue(Integer.class);
        Integer pwm = e.child("pwm_fans").getValue(Integer.class);
        Double objetivo = e.child("setpoint").getValue(Double.class);
        Long uptime = e.child("uptime_s").getValue(Long.class);
        String m = e.child("modo").getValue(String.class);
        String motivo = e.child("motivo").getValue(String.class);
        String textoAlerta = e.child("alerta").getValue(String.class);

        int nivel = n == null ? 0 : n;
        int alerta = a == null ? Contrato.ALERTA_NINGUNA : a;
        if (objetivo != null) setpoint = objetivo;
        if (Contrato.esModoValido(m)) modo = m;

        int color = ContextCompat.getColor(this, Colores.nivel(nivel));
        txtNivel.setText(nivel == 0 ? "--" : "N" + nivel);
        txtNivel.setTextColor(color);
        cardNivel.setStrokeColor(color);
        txtNivelDetalle.setText(Contrato.nombreNivel(nivel));
        txtMotivo.setText(motivo == null ? "" : motivo);
        txtObjetivo.setText(grados(setpoint));

        pintarIndicador(indCeldaA, getString(R.string.celda_a), verdadero(e, "celda_a"), color);
        pintarIndicador(indCeldaB, getString(R.string.celda_b), verdadero(e, "celda_b"), color);
        pintarIndicador(indBomba, getString(R.string.bomba), verdadero(e, "bomba"), color);
        boolean ventilando = pwm != null && pwm > 0;
        pintarIndicador(indVentiladores, getString(R.string.ventiladores), ventilando, color);
        if (ventilando) indVentiladores.setText(getString(R.string.ventiladores_pct, pwm));

        if (uptime != null && uptime != uptimeAnterior) {
            if (uptimeAnterior >= 0) ultimoLatido = System.currentTimeMillis();
            uptimeAnterior = uptime;
        }
        txtModo.setText(Contrato.nombreModo(modo));
        repo.recordarEstado(nivel, alerta, objetivo);
        espejo.setEstado(mapa(e));
        mostrarAlerta(alerta, textoAlerta);
    }

    private void pintarDispositivo(DataSnapshot d) {
        String ip = d.child("ip").getValue(String.class);
        Long rssi = d.child("rssi").getValue(Long.class);
        String firmware = d.child("firmware").getValue(String.class);
        Long caidas = d.child("caidas_wifi").getValue(Long.class);
        txtDispositivo.setText(getString(R.string.dispositivo_detalle, texto(ip), texto(rssi),
                texto(firmware), texto(caidas)));
        espejo.setDispositivo(mapa(d));
    }

    private void pintarEventos(DataSnapshot eventos) {
        List<DataSnapshot> lista = new ArrayList<>();
        for (DataSnapshot e : eventos.getChildren()) lista.add(0, e);   // el más reciente arriba
        listaEventos.removeAllViews();
        LayoutInflater inflador = LayoutInflater.from(this);
        for (DataSnapshot e : lista) {
            String tipo = e.child("tipo").getValue(String.class);
            Long ts = e.child("ts").getValue(Long.class);
            View fila = inflador.inflate(R.layout.item_historial, listaEventos, false);
            ((TextView) fila.findViewById(R.id.txt_titulo)).setText(e.child("texto").getValue(String.class));
            ((TextView) fila.findViewById(R.id.txt_detalle)).setText(getString(R.string.evento_detalle,
                    Contrato.nombreTipoEvento(tipo), ts == null ? "--" : hora.format(new Date(ts))));
            fila.findViewById(R.id.barra).setBackgroundColor(ContextCompat.getColor(this, Colores.tipoEvento(tipo)));
            listaEventos.addView(fila);
        }
        txtSinEventos.setVisibility(lista.isEmpty() ? View.VISIBLE : View.GONE);
        List<Map<String, Object>> mapas = new ArrayList<>();
        for (DataSnapshot e : lista) mapas.add(mapa(e));
        espejo.setEventos(mapas);
    }

    /** .info/connected: si el teléfono pierde Firebase más de 3 s, se avisa; la caché sigue mostrando datos. */
    private void pintarConexion(DataSnapshot s) {
        boolean conectado = Boolean.TRUE.equals(s.getValue(Boolean.class));
        espejo.setConectado(conectado);
        reloj.removeCallbacks(avisarSinConexion);
        if (conectado) {
            txtConexion.setVisibility(View.GONE);
            if (avisoSinConexion != null) avisoSinConexion.dismiss();
            if (enModoOffline) {                     // volvió la red: avisar y quitar la notificación
                enModoOffline = false;
                Notificaciones.quitarModoOffline(this);
                Snackbar.make(raiz, R.string.conexion_recuperada, Snackbar.LENGTH_LONG)
                        .setBackgroundTint(ContextCompat.getColor(this, R.color.verde)).show();
            }
        } else {
            reloj.postDelayed(avisarSinConexion, ESPERA_SIN_CONEXION_MS);
        }
    }

    /** Modo offline: insignia, Snackbar con acceso al historial (Room) y notificación del sistema. */
    private final Runnable avisarSinConexion = () -> {
        enModoOffline = true;
        txtConexion.setVisibility(View.VISIBLE);
        avisoSinConexion = Snackbar.make(raiz, R.string.sin_conexion_detalle, Snackbar.LENGTH_INDEFINITE)
                .setAction(R.string.accion_historial, v -> abrirHistorial());
        avisoSinConexion.show();
        Notificaciones.modoOffline(this);
    };

    // ------------------------------------------------------------ alertas

    /** Banner, alarma, notificación y diálogo cuando aparece una alerta nueva. */
    private void mostrarAlerta(int alerta, String textoEsp32) {
        boolean hay = alerta != Contrato.ALERTA_NINGUNA;
        String nombre = Contrato.nombreAlerta(alerta);
        cardAlerta.setVisibility(hay ? View.VISIBLE : View.GONE);
        txtAlerta.setText(textoEsp32 == null || textoEsp32.isEmpty() ? nombre : textoEsp32);

        if (hay && alerta != alertaAnterior) {
            Alarma.sonar(this, alerta);
            Notificaciones.alerta(this, getString(R.string.alerta_froster), nombre);
            if (dialogoAlerta != null) dialogoAlerta.dismiss();
            dialogoAlerta = new AlertDialog.Builder(this)
                    .setIcon(R.drawable.ic_alerta_rojo)
                    .setTitle(R.string.alerta_froster)
                    .setMessage(getString(R.string.alerta_detalle, nombre, Contrato.queHacer(alerta)))
                    .setPositiveButton(R.string.entendido, null)
                    .show();
        } else if (!hay && alertaAnterior != Contrato.ALERTA_NINGUNA) {
            if (dialogoAlerta != null) dialogoAlerta.dismiss();
            Snackbar.make(raiz, R.string.alerta_resuelta, Snackbar.LENGTH_LONG)
                    .setBackgroundTint(ContextCompat.getColor(this, R.color.verde))
                    .setTextColor(ContextCompat.getColor(this, R.color.blanco))
                    .show();
        }
        alertaAnterior = alerta;
    }

    /** Cada segundo: ¿el ESP32 sigue publicando? (su uptime_s cambia al menos cada 10 s) */
    private final Runnable revisarLatido = new Runnable() {
        @Override public void run() {
            boolean vivo = ultimoLatido > 0
                    && System.currentTimeMillis() - ultimoLatido <= Contrato.LATIDO_MAXIMO_MS;
            txtLatido.setText(vivo ? R.string.en_vivo : R.string.sin_datos_esp32);
            espejo.setVivo(vivo);
            txtLatido.setBackgroundTintList(ColorStateList.valueOf(
                    ContextCompat.getColor(MainActivity.this, vivo ? R.color.verde : R.color.ambar)));
            reloj.postDelayed(this, 1000);
        }
    };

    // ------------------------------------------------------------ comandos

    private void cambiarObjetivo(double delta) {
        double nuevo = Contrato.ajustarSetpoint(setpoint + delta);
        if (nuevo == setpoint) {
            Toast.makeText(this, getString(R.string.objetivo_limite, nuevo), Toast.LENGTH_SHORT).show();
            return;
        }
        mandar(Collections.singletonMap("setpoint", nuevo), getString(R.string.objetivo_valor, nuevo));
    }

    private void cambiarModo(String nuevo) {
        if (!Contrato.esModoValido(nuevo) || nuevo.equals(modo)) return;
        mandar(Collections.singletonMap("modo", nuevo), getString(R.string.modo_valor, Contrato.nombreModo(nuevo)));
    }

    /** Manda el comando y avisa con un Snackbar cuando el ESP32 lo confirma en ack/ (o si no lo hace). */
    private void mandar(Map<String, Object> cambios, String queSePidio) {
        if (enModoOffline) {                  // sin red la orden no llegaría: se dice en vez de esperar 5 s
            Toast.makeText(this, R.string.sin_red_para_ordenes, Toast.LENGTH_SHORT).show();
            return;
        }
        Map<String, Object> orden = new HashMap<>(cambios);
        orden.put("origen", "app");           // el tablero muestra que el cambio vino de la app
        repo.enviarComando(orden, new Repositorio.ResultadoComando() {
            @Override public void confirmado(long ms) {
                if (isFinishing() || isDestroyed()) return;
                txtLatencia.setText(getString(R.string.ultimo_comando, ms));
                espejo.setUltimoComando(getString(R.string.confirmado_en, queSePidio, ms));
                Snackbar.make(raiz, getString(R.string.confirmado_en, queSePidio, ms), Snackbar.LENGTH_SHORT).show();
            }
            @Override public void fallo(String motivo) {
                if (isFinishing() || isDestroyed()) return;
                Snackbar.make(raiz, motivo, Snackbar.LENGTH_LONG)
                        .setBackgroundTint(ContextCompat.getColor(MainActivity.this, R.color.rojo))
                        .setTextColor(ContextCompat.getColor(MainActivity.this, R.color.blanco))
                        .show();
            }
        });
    }

    // ------------------------------------------------------------ utilidades

    private void pintarIndicador(TextView v, String nombre, boolean encendido, int colorEncendido) {
        v.setText(getString(R.string.indicador, nombre,
                getString(encendido ? R.string.encendido : R.string.apagado)));
        v.setBackgroundTintList(ColorStateList.valueOf(encendido ? colorEncendido
                : ContextCompat.getColor(this, R.color.apagado_fondo)));
        v.setTextColor(ContextCompat.getColor(this, encendido ? R.color.blanco : R.color.apagado_texto));
    }

    /** Un nodo de Firebase como mapa (vacío si no existe todavía). */
    @SuppressWarnings("unchecked")
    private static Map<String, Object> mapa(DataSnapshot s) {
        Object v = s.getValue();
        return v instanceof Map ? (Map<String, Object>) v : Collections.emptyMap();
    }

    private static Double numero(DataSnapshot s, String clave) {
        return s.child(clave).getValue(Double.class);
    }

    private static boolean verdadero(DataSnapshot s, String clave) {
        return Boolean.TRUE.equals(s.child(clave).getValue(Boolean.class));
    }

    private static String grados(Double v) {
        return v == null ? "--" : String.format(Locale.getDefault(), "%.1f °C", v);
    }

    private static String texto(Object v) {
        return v == null ? "--" : String.valueOf(v);
    }

    private void abrirHistorial() {
        startActivity(new Intent(this, HistorialActivity.class));
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.accion_historial) {
            abrirHistorial();
            return true;
        }
        if (item.getItemId() == R.id.accion_vista) {
            verClasica = !verClasica;
            vistaClasica.setVisibility(verClasica ? View.VISIBLE : View.GONE);
            findViewById(R.id.panel_espejo).setVisibility(verClasica ? View.GONE : View.VISIBLE);
            findViewById(R.id.txt_espejo).setVisibility(verClasica ? View.GONE : View.VISIBLE);
            item.setTitle(verClasica ? R.string.vista_panel : R.string.vista_clasica);
            return true;
        }
        if (item.getItemId() == R.id.accion_salir) {
            FirebaseAuth.getInstance().signOut();
            startActivity(new Intent(this, LoginActivity.class));
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
