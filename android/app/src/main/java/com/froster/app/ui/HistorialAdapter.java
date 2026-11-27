package com.froster.app.ui;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;

import com.froster.app.R;

import java.util.ArrayList;
import java.util.List;

/** Filas de dos líneas (qué pasó y cuándo) con una barra de color según el tipo. */
public class HistorialAdapter extends RecyclerView.Adapter<HistorialAdapter.Fila> {

    /** Una fila del historial. tipo: "nivel", "alerta", "pico" o null. */
    public static final class Dato {
        final String titulo, detalle, tipo;
        public Dato(String titulo, String detalle, String tipo) {
            this.titulo = titulo;
            this.detalle = detalle;
            this.tipo = tipo;
        }
    }

    private final List<Dato> filas = new ArrayList<>();

    public void cambiar(List<Dato> nuevas) {
        filas.clear();
        filas.addAll(nuevas);
        notifyDataSetChanged();
    }

    @NonNull @Override
    public Fila onCreateViewHolder(@NonNull ViewGroup padre, int tipo) {
        return new Fila(LayoutInflater.from(padre.getContext()).inflate(R.layout.item_historial, padre, false));
    }

    @Override
    public void onBindViewHolder(@NonNull Fila f, int i) {
        Dato d = filas.get(i);
        f.titulo.setText(d.titulo);
        f.detalle.setText(d.detalle);
        f.barra.setBackgroundColor(ContextCompat.getColor(f.itemView.getContext(), Colores.tipoEvento(d.tipo)));
    }

    @Override
    public int getItemCount() { return filas.size(); }

    static class Fila extends RecyclerView.ViewHolder {
        final TextView titulo, detalle;
        final View barra;
        Fila(View v) {
            super(v);
            titulo = v.findViewById(R.id.txt_titulo);
            detalle = v.findViewById(R.id.txt_detalle);
            barra = v.findViewById(R.id.barra);
        }
    }
}
