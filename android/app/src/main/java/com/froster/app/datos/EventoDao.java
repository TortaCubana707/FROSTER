package com.froster.app.datos;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;

import java.util.List;

@Dao
public interface EventoDao {
    @Insert(onConflict = OnConflictStrategy.IGNORE)
    void insertar(Evento evento);

    @Query("SELECT * FROM eventos ORDER BY ts DESC LIMIT :limite")
    List<Evento> ultimos(int limite);

    /** Picos de temperatura excedidos (criterio del Proyecto 1). */
    @Query("SELECT * FROM eventos WHERE tipo = 'pico' ORDER BY ts DESC")
    List<Evento> picos();
}
