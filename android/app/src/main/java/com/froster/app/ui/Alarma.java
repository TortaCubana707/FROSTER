package com.froster.app.ui;

import android.content.Context;
import android.media.AudioManager;
import android.media.ToneGenerator;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;

import com.froster.app.datos.Contrato;

/**
 * Alarma dentro de la app: un tono y una vibración distintos por alerta, igual que el buzzer
 * del ESP32 tiene un sonido por evento. Se oye aunque el teléfono tenga las notificaciones
 * en silencio, porque usa el canal de alarma.
 */
public final class Alarma {
    private Alarma() { }

    private static final int DURACION_MS = 1500;
    private static final int VOLUMEN = 90;   // 0..100

    public static void sonar(Context contexto, int alertaId) {
        if (alertaId == Contrato.ALERTA_NINGUNA) return;
        vibrar(contexto, Contrato.patronVibracion(alertaId));
        try {
            ToneGenerator tono = new ToneGenerator(AudioManager.STREAM_ALARM, VOLUMEN);
            tono.startTone(tipoTono(alertaId), DURACION_MS);
            new Handler(Looper.getMainLooper()).postDelayed(tono::release, DURACION_MS + 200);
        } catch (RuntimeException e) {
            // Si el audio está ocupado (por ejemplo, en una llamada) queda la vibración y la notificación.
        }
    }

    private static int tipoTono(int alertaId) {
        switch (alertaId) {
            case Contrato.ALERTA_CONGELAMIENTO: return ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD;   // agudos
            case Contrato.ALERTA_SIN_HIELO: return ToneGenerator.TONE_PROP_BEEP2;                  // 2 pitidos
            case Contrato.ALERTA_SENSOR: return ToneGenerator.TONE_SUP_ERROR;                      // grave
            default: return ToneGenerator.TONE_CDMA_EMERGENCY_RINGBACK;                            // alternado
        }
    }

    @SuppressWarnings("deprecation")   // VIBRATOR_SERVICE sigue funcionando en Android 12+
    private static void vibrar(Context contexto, long[] patron) {
        Vibrator vibrador = (Vibrator) contexto.getSystemService(Context.VIBRATOR_SERVICE);
        if (vibrador == null || !vibrador.hasVibrator() || patron.length == 0) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrador.vibrate(VibrationEffect.createWaveform(patron, -1));
        } else {
            vibrador.vibrate(patron, -1);
        }
    }
}
