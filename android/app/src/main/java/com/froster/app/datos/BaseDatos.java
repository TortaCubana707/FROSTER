package com.froster.app.datos;

import android.content.Context;

import androidx.room.Database;
import androidx.room.Room;
import androidx.room.RoomDatabase;

/** Base local de Room. Una sola instancia para toda la app. */
@Database(entities = {Lectura.class, Evento.class}, version = 1)
public abstract class BaseDatos extends RoomDatabase {
    public abstract LecturaDao lecturas();
    public abstract EventoDao eventos();

    private static volatile BaseDatos instancia;

    public static BaseDatos de(Context contexto) {
        if (instancia == null) {
            synchronized (BaseDatos.class) {
                if (instancia == null) {
                    instancia = Room.databaseBuilder(contexto.getApplicationContext(),
                            BaseDatos.class, "froster.db").build();
                }
            }
        }
        return instancia;
    }
}
