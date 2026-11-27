package com.froster.app.datos;

import android.content.Context;

import androidx.room.Database;
import androidx.room.Room;
import androidx.room.RoomDatabase;
import androidx.room.migration.Migration;
import androidx.sqlite.db.SupportSQLiteDatabase;

/** Base local de Room. Una sola instancia para toda la app. */
@Database(entities = {Lectura.class, Evento.class}, version = 2)
public abstract class BaseDatos extends RoomDatabase {
    public abstract LecturaDao lecturas();
    public abstract EventoDao eventos();

    private static volatile BaseDatos instancia;

    /** v1 -> v2: objetivo y marca de pico excedido en cada lectura, SIN borrar el historial existente. */
    static final Migration MIGRACION_1_2 = new Migration(1, 2) {
        @Override
        public void migrate(SupportSQLiteDatabase db) {
            db.execSQL("ALTER TABLE lecturas ADD COLUMN objetivo REAL");
            db.execSQL("ALTER TABLE lecturas ADD COLUMN excede INTEGER NOT NULL DEFAULT 0");
        }
    };

    public static BaseDatos de(Context contexto) {
        if (instancia == null) {
            synchronized (BaseDatos.class) {
                if (instancia == null) {
                    instancia = Room.databaseBuilder(contexto.getApplicationContext(),
                            BaseDatos.class, "froster.db")
                            .addMigrations(MIGRACION_1_2)
                            .build();
                }
            }
        }
        return instancia;
    }
}
