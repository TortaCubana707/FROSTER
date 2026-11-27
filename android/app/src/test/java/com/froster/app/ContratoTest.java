package com.froster.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.froster.app.datos.Contrato;

import org.junit.Test;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** El contrato debe coincidir con el firmware y con docs/CONTRATO_FIREBASE.md. */
public class ContratoTest {

    @Test
    public void lasRutasCuelganDeFroster() {
        assertEquals("froster/config", Contrato.CONFIG);
        assertEquals("froster/ack", Contrato.ACK);
    }

    @Test
    public void elObjetivoSeQuedaEnElRangoQueAceptaElEsp32() {
        assertEquals(15.0, Contrato.ajustarSetpoint(10), 0.001);
        assertEquals(40.0, Contrato.ajustarSetpoint(55), 0.001);
        assertEquals(26.5, Contrato.ajustarSetpoint(26.4), 0.001);
    }

    @Test
    public void cadaAlertaTieneNombre() {
        assertEquals("Fuga de agua junto a la bomba", Contrato.nombreAlerta(Contrato.ALERTA_FUGA));
        assertEquals("Sin alertas", Contrato.nombreAlerta(Contrato.ALERTA_NINGUNA));
    }

    @Test
    public void soloAceptaLosModosQueEntiendeElEsp32() {
        assertTrue(Contrato.esModoValido("auto"));
        assertTrue(Contrato.esModoValido("manual"));
        assertTrue(Contrato.esModoValido("off"));
        assertFalse(Contrato.esModoValido("turbo"));
        assertFalse(Contrato.esModoValido(null));
    }

    @Test
    public void cadaAlertaVibraDistintoYSinAlertaNoVibra() {
        assertEquals(0, Contrato.patronVibracion(Contrato.ALERTA_NINGUNA).length);
        Set<String> patrones = new HashSet<>();
        for (int id = Contrato.ALERTA_CONGELAMIENTO; id <= Contrato.ALERTA_FUGA; id++) {
            patrones.add(Arrays.toString(Contrato.patronVibracion(id)));
        }
        assertEquals(4, patrones.size());
    }

    @Test
    public void cadaAlertaDiceQueHacer() {
        for (int id = Contrato.ALERTA_CONGELAMIENTO; id <= Contrato.ALERTA_FUGA; id++) {
            assertFalse(Contrato.queHacer(id).isEmpty());
        }
        assertTrue(Contrato.queHacer(Contrato.ALERTA_NINGUNA).isEmpty());
    }
}
