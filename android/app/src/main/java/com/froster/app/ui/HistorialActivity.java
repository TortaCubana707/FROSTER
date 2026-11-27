package com.froster.app.ui;

import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.froster.app.R;
import com.froster.app.datos.Evento;
import com.froster.app.datos.Lectura;
import com.froster.app.datos.Repositorio;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/** Historial guardado en Room: se puede ver sin internet. */
public class HistorialActivity extends AppCompatActivity {

    private HistorialAdapter adaptador;
    private TextView vacio;
    private Repositorio repo;
    private final DateFormat hora = DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.MEDIUM);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_historial);
        repo = Repositorio.de(this);
        vacio = findViewById(R.id.txt_vacio);
        RecyclerView lista = findViewById(R.id.lista);
        lista.setLayoutManager(new LinearLayoutManager(this));
        adaptador = new HistorialAdapter();
        lista.setAdapter(adaptador);

        Button eventos = findViewById(R.id.btn_eventos);
        Button lecturas = findViewById(R.id.btn_lecturas);
        eventos.setOnClickListener(v -> mostrarEventos());
        lecturas.setOnClickListener(v -> mostrarLecturas());
        mostrarEventos();
    }

    private void mostrarEventos() {
        repo.ultimosEventos(100, eventos -> {
            List<String[]> filas = new ArrayList<>();
            for (Evento e : eventos) filas.add(new String[]{ e.texto, hora.format(new Date(e.ts)) });
            mostrar(filas);
        });
    }

    private void mostrarLecturas() {
        repo.ultimasLecturas(100, lecturas -> {
            List<String[]> filas = new ArrayList<>();
            for (Lectura l : lecturas) {
                filas.add(new String[]{
                        String.format(Locale.getDefault(), "Cámara %s · bloque %s · nivel %d",
                                texto(l.ambiente), texto(l.placaFria), l.nivel),
                        hora.format(new Date(l.ts)) });
            }
            mostrar(filas);
        });
    }

    private void mostrar(List<String[]> filas) {
        adaptador.cambiar(filas);
        vacio.setVisibility(filas.isEmpty() ? TextView.VISIBLE : TextView.GONE);
    }

    private static String texto(Double v) {
        return v == null ? "--" : String.format(Locale.getDefault(), "%.1f °C", v);
    }
}
