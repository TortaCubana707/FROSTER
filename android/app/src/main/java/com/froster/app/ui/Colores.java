package com.froster.app.ui;

import androidx.annotation.ColorRes;

import com.froster.app.R;

/** Un solo lugar para los colores con significado: mismos que el LED RGB del prototipo. */
final class Colores {
    private Colores() { }

    @ColorRes
    static int nivel(int nivel) {
        if (nivel == 1) return R.color.nivel1;
        if (nivel == 2) return R.color.nivel2;
        if (nivel == 3) return R.color.nivel3;
        return R.color.tenue;
    }

    /** El mismo color de nivel, en ARGB (para vistas armadas en código). */
    static int argbNivel(int nivel) {
        if (nivel == 1) return 0xFF2DA44E;
        if (nivel == 2) return 0xFF1B9AAA;
        if (nivel == 3) return 0xFF1F6FEB;
        return 0xFF6E7781;
    }

    @ColorRes
    static int tipoEvento(String tipo) {
        if ("alerta".equals(tipo)) return R.color.rojo;
        if ("pico".equals(tipo)) return R.color.ambar;
        if ("nivel".equals(tipo)) return R.color.frio;
        return R.color.tenue;
    }
}
