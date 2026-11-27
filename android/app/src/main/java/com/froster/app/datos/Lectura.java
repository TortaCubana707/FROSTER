package com.froster.app.datos;

import androidx.room.Entity;
import androidx.room.PrimaryKey;

/** Una muestra de los sensores guardada en el teléfono (historial offline). */
@Entity(tableName = "lecturas")
public class Lectura {
    @PrimaryKey(autoGenerate = true)
    public long id;
    public long ts;              // ms desde 1970 (hora del ESP32, o del teléfono si no venía)
    public Double ambiente;      // °C; null si el sensor no mandó dato
    public Double humedad;       // %
    public Double placaFria;     // °C
    public Double fugaPct;       // %
    public int nivel;
    public int alertaId;
}
