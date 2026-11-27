package com.froster.app.datos;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.Query;

import java.util.List;

@Dao
public interface LecturaDao {
    @Insert
    long insertar(Lectura lectura);

    @Query("SELECT * FROM lecturas ORDER BY ts DESC LIMIT :limite")
    List<Lectura> ultimas(int limite);

    @Query("SELECT COUNT(*) FROM lecturas")
    int contar();

    /** Picos de temperatura excedidos: lecturas en que la cámara superó el objetivo del usuario. */
    @Query("SELECT * FROM lecturas WHERE excede = 1 ORDER BY ts DESC LIMIT :limite")
    List<Lectura> excedidas(int limite);
}
