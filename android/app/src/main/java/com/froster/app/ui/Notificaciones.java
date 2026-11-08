package com.froster.app.ui;

import android.Manifest;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.core.content.ContextCompat;

import com.froster.app.R;

/** Notificación con sonido para las alertas críticas del ESP32. */
public final class Notificaciones {
    private Notificaciones() { }

    public static final String CANAL_ALERTAS = "alertas";
    private static final int ID_ALERTA = 1;

    public static void crearCanal(Context contexto) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;
        NotificationChannel canal = new NotificationChannel(CANAL_ALERTAS,
                contexto.getString(R.string.canal_alertas), NotificationManager.IMPORTANCE_HIGH);
        canal.setDescription(contexto.getString(R.string.canal_alertas_desc));
        canal.enableVibration(true);
        contexto.getSystemService(NotificationManager.class).createNotificationChannel(canal);
    }

    public static void alerta(Context contexto, String titulo, String texto) {
        if (Build.VERSION.SDK_INT >= 33 && ContextCompat.checkSelfPermission(contexto,
                Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) return;
        Intent abrir = new Intent(contexto, MainActivity.class)
                .setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        PendingIntent pendiente = PendingIntent.getActivity(contexto, 0, abrir,
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        NotificationCompat.Builder n = new NotificationCompat.Builder(contexto, CANAL_ALERTAS)
                .setSmallIcon(R.drawable.ic_alerta)
                .setContentTitle(titulo)
                .setContentText(texto)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setDefaults(NotificationCompat.DEFAULT_ALL)     // sonido y vibración
                .setContentIntent(pendiente)
                .setAutoCancel(true);
        NotificationManagerCompat.from(contexto).notify(ID_ALERTA, n.build());
    }
}
