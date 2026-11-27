package com.froster.app.datos;

import androidx.annotation.NonNull;
import androidx.room.Entity;
import androidx.room.PrimaryKey;

/** Un evento de froster/eventos: cambio de nivel, alerta o pico de temperatura. */
@Entity(tableName = "eventos")
public class Evento {
    @PrimaryKey
    @NonNull
    public String clave = "";    // id del push en Firebase: evita guardar el mismo evento dos veces
    public String tipo;          // "nivel", "alerta" o "pico"
    public String texto;
    public int nivel;
    public Double camara;
    public Double setpoint;
    public long ts;
}
