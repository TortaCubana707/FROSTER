package com.froster.app.ui;

import android.Manifest;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import com.froster.app.R;
import com.froster.app.datos.Contrato;
import com.froster.app.datos.Repositorio;
import com.google.android.material.snackbar.Snackbar;
import com.google.firebase.auth.FirebaseAuth;
import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.ValueEventListener;

import java.util.Collections;
import java.util.Locale;

/**
 * Pantalla principal: datos en vivo del ESP32, control del objetivo y avisos.
 * E3: aquí se completa el maquetado (modo, celdas en manual, gráfica...).
 */
public class MainActivity extends AppCompatActivity {

    private Repositorio repo;
    private ValueEventListener oyenteLecturas, oyenteEstado;
    private TextView txtNivel, txtNivelDetalle, txtCamara, txtHumedad, txtBloque, txtFuga,
            txtObjetivo, txtMotivo, txtAlerta, txtLatido;
    private View raiz;

    private final Handler reloj = new Handler(Looper.getMainLooper());
    private long ultimoLatido = 0, uptimeAnterior = -1;
    private int alertaAnterior = Contrato.ALERTA_NINGUNA;
    private double setpoint = 28.0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        repo = Repositorio.de(this);

        raiz = findViewById(R.id.raiz);
        txtNivel = findViewById(R.id.txt_nivel);
        txtNivelDetalle = findViewById(R.id.txt_nivel_detalle);
        txtCamara = findViewById(R.id.txt_camara);
        txtHumedad = findViewById(R.id.txt_humedad);
        txtBloque = findViewById(R.id.txt_bloque);
        txtFuga = findViewById(R.id.txt_fuga);
        txtObjetivo = findViewById(R.id.txt_objetivo);
        txtMotivo = findViewById(R.id.txt_motivo);
        txtAlerta = findViewById(R.id.txt_alerta);
        txtLatido = findViewById(R.id.txt_latido);
        Button menos = findViewById(R.id.btn_menos);
        Button mas = findViewById(R.id.btn_mas);
        menos.setOnClickListener(v -> cambiarObjetivo(-0.5));
        mas.setOnClickListener(v -> cambiarObjetivo(+0.5));

        if (Build.VERSION.SDK_INT >= 33 && ContextCompat.checkSelfPermission(this,
                Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 1);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        oyenteLecturas = repo.escuchar(Contrato.LECTURAS, this::pintarLecturas);
        oyenteEstado = repo.escuchar(Contrato.ESTADO, this::pintarEstado);
        reloj.post(revisarLatido);
    }

    @Override
    protected void onStop() {
        super.onStop();
        repo.dejarDeEscuchar(Contrato.LECTURAS, oyenteLecturas);
        repo.dejarDeEscuchar(Contrato.ESTADO, oyenteEstado);
        reloj.removeCallbacks(revisarLatido);
    }

    private void pintarLecturas(DataSnapshot l) {
        txtCamara.setText(grados(l.child("ambiente").getValue(Double.class)));
        Double humedad = l.child("humedad").getValue(Double.class);
        txtHumedad.setText(humedad == null ? "--" : getString(R.string.humedad_pct, humedad));
        txtBloque.setText(grados(l.child("placa_fria").getValue(Double.class)));
        Double fuga = l.child("fuga_pct").getValue(Double.class);
        txtFuga.setText(fuga == null ? "--" : getString(R.string.fuga_pct, fuga));
        repo.guardarLectura(l);
    }

    private void pintarEstado(DataSnapshot e) {
        Integer nivel = e.child("nivel").getValue(Integer.class);
        Integer alerta = e.child("alerta_id").getValue(Integer.class);
        Double objetivo = e.child("setpoint").getValue(Double.class);
        Long uptime = e.child("uptime_s").getValue(Long.class);
        int n = nivel == null ? 0 : nivel;
        int a = alerta == null ? Contrato.ALERTA_NINGUNA : alerta;

        txtNivel.setText(n == 0 ? "--" : "N" + n);
        txtNivelDetalle.setText(Contrato.nombreNivel(n));
        if (objetivo != null) setpoint = objetivo;
        txtObjetivo.setText(grados(setpoint));
        txtMotivo.setText(e.child("motivo").getValue(String.class));
        if (uptime != null && uptime != uptimeAnterior) {
            if (uptimeAnterior >= 0) ultimoLatido = System.currentTimeMillis();
            uptimeAnterior = uptime;
        }
        repo.recordarEstado(n, a);
        mostrarAlerta(a);
    }

    /** Banner, notificación con sonido y diálogo cuando aparece una alerta nueva. */
    private void mostrarAlerta(int alerta) {
        txtAlerta.setVisibility(alerta == Contrato.ALERTA_NINGUNA ? View.GONE : View.VISIBLE);
        txtAlerta.setText(Contrato.nombreAlerta(alerta));
        if (alerta != Contrato.ALERTA_NINGUNA && alerta != alertaAnterior) {
            String nombre = Contrato.nombreAlerta(alerta);
            Notificaciones.alerta(this, getString(R.string.alerta_froster), nombre);
            new AlertDialog.Builder(this)
                    .setTitle(R.string.alerta_froster)
                    .setMessage(nombre)
                    .setPositiveButton(R.string.entendido, null)
                    .show();
        } else if (alerta == Contrato.ALERTA_NINGUNA && alertaAnterior != Contrato.ALERTA_NINGUNA) {
            Snackbar.make(raiz, R.string.alerta_resuelta, Snackbar.LENGTH_LONG).show();
        }
        alertaAnterior = alerta;
    }

    private void cambiarObjetivo(double delta) {
        double nuevo = Contrato.ajustarSetpoint(setpoint + delta);
        repo.enviarComando(Collections.singletonMap("setpoint", nuevo), new Repositorio.ResultadoComando() {
            @Override public void confirmado(long ms) {
                Snackbar.make(raiz, getString(R.string.comando_confirmado, nuevo, ms), Snackbar.LENGTH_SHORT).show();
            }
            @Override public void fallo(String motivo) {
                Snackbar.make(raiz, motivo, Snackbar.LENGTH_LONG).show();
            }
        });
    }

    /** Cada segundo: ¿el ESP32 sigue publicando? */
    private final Runnable revisarLatido = new Runnable() {
        @Override public void run() {
            boolean vivo = ultimoLatido > 0
                    && System.currentTimeMillis() - ultimoLatido <= Contrato.LATIDO_MAXIMO_MS;
            txtLatido.setText(vivo ? R.string.en_vivo : R.string.sin_datos_esp32);
            txtLatido.setTextColor(ContextCompat.getColor(MainActivity.this,
                    vivo ? R.color.verde : R.color.ambar));
            reloj.postDelayed(this, 1000);
        }
    };

    private static String grados(Double v) {
        return v == null ? "--" : String.format(Locale.getDefault(), "%.1f °C", v);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.accion_historial) {
            startActivity(new Intent(this, HistorialActivity.class));
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
