package com.froster.app.ui;

import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.froster.app.R;
import com.froster.app.datos.Contrato;
import com.froster.app.datos.Evento;
import com.froster.app.datos.Lectura;
import com.froster.app.datos.Repositorio;
import com.google.android.material.button.MaterialButtonToggleGroup;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Historial guardado en Room: se puede ver sin internet.
 * Tres pestañas: eventos del ESP32, lecturas guardadas cada 30 s y picos de temperatura.
 */
public class HistorialActivity extends AppCompatActivity {

    private static final int LIMITE = 100;

    private HistorialAdapter adaptador;
    private TextView vacio, resumen;
    private Repositorio repo;
    private final DateFormat hora = DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.MEDIUM);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_historial);
        if (getSupportActionBar() != null) getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        repo = Repositorio.de(this);
        vacio = findViewById(R.id.txt_vacio);
        resumen = findViewById(R.id.txt_resumen);
        RecyclerView lista = findViewById(R.id.lista);
        lista.setLayoutManager(new LinearLayoutManager(this));
        lista.addItemDecoration(new DividerItemDecoration(this, DividerItemDecoration.VERTICAL));
        adaptador = new HistorialAdapter();
        lista.setAdapter(adaptador);

        MaterialButtonToggleGroup pestanas = findViewById(R.id.pestanas);
        pestanas.addOnButtonCheckedListener((grupo, id, marcado) -> {
            if (!marcado) return;
            if (id == R.id.btn_lecturas) mostrarLecturas();
            else if (id == R.id.btn_picos) mostrarPicos();
            else mostrarEventos();
        });
        pestanas.check(R.id.btn_eventos);
    }

    private void mostrarEventos() {
        repo.ultimosEventos(LIMITE, eventos -> mostrar(filasDeEventos(eventos)));
    }

    /** Picos: los que reportó el ESP32 (eventos) y las lecturas guardadas sobre el objetivo (Room v2). */
    private void mostrarPicos() {
        repo.picos(picos -> repo.lecturasExcedidas(LIMITE, excedidas -> {
            List<HistorialAdapter.Dato> filas = filasDeEventos(picos);
            for (Lectura l : excedidas) {
                filas.add(new HistorialAdapter.Dato(
                        getString(R.string.pico_local, grados(l.ambiente), grados(l.objetivo)),
                        hora.format(new Date(l.ts)) + " · " + getString(R.string.guardado_telefono), "pico"));
            }
            mostrar(filas);
        }));
    }

    private void mostrarLecturas() {
        repo.ultimasLecturas(LIMITE, lecturas -> {
            List<HistorialAdapter.Dato> filas = new ArrayList<>();
            for (Lectura l : lecturas) {
                String titulo = getString(R.string.lectura_fila, grados(l.ambiente), grados(l.placaFria), l.nivel);
                String detalle = hora.format(new Date(l.ts));
                if (l.alertaId != Contrato.ALERTA_NINGUNA) detalle += " · " + Contrato.nombreAlerta(l.alertaId);
                if (l.excede) detalle += " · " + getString(R.string.sobre_objetivo, grados(l.objetivo));
                filas.add(new HistorialAdapter.Dato(titulo, detalle,
                        l.alertaId != Contrato.ALERTA_NINGUNA ? "alerta" : l.excede ? "pico" : null));
            }
            mostrar(filas);
        });
    }

    private List<HistorialAdapter.Dato> filasDeEventos(List<Evento> eventos) {
        List<HistorialAdapter.Dato> filas = new ArrayList<>();
        for (Evento e : eventos) {
            String detalle = getString(R.string.evento_detalle, Contrato.nombreTipoEvento(e.tipo),
                    hora.format(new Date(e.ts)));
            if (e.camara != null) detalle += " · cámara " + grados(e.camara);
            filas.add(new HistorialAdapter.Dato(e.texto, detalle, e.tipo));
        }
        return filas;
    }

    private void mostrar(List<HistorialAdapter.Dato> filas) {
        adaptador.cambiar(filas);
        resumen.setText(getString(R.string.resumen_historial, filas.size()));
        vacio.setVisibility(filas.isEmpty() ? View.VISIBLE : View.GONE);
    }

    private static String grados(Double v) {
        return v == null ? "--" : String.format(Locale.getDefault(), "%.1f °C", v);
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
