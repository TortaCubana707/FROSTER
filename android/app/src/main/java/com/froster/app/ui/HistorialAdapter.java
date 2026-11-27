package com.froster.app.ui;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.froster.app.R;

import java.util.ArrayList;
import java.util.List;

/** Filas de dos líneas: qué pasó y cuándo. */
public class HistorialAdapter extends RecyclerView.Adapter<HistorialAdapter.Fila> {

    private final List<String[]> filas = new ArrayList<>();

    public void cambiar(List<String[]> nuevas) {
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
        f.titulo.setText(filas.get(i)[0]);
        f.detalle.setText(filas.get(i)[1]);
    }

    @Override
    public int getItemCount() { return filas.size(); }

    static class Fila extends RecyclerView.ViewHolder {
        final TextView titulo, detalle;
        Fila(View v) {
            super(v);
            titulo = v.findViewById(R.id.txt_titulo);
            detalle = v.findViewById(R.id.txt_detalle);
        }
    }
}
