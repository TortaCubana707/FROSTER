package com.froster.app;

import static org.junit.Assert.assertEquals;

import com.froster.app.datos.Contrato;

import org.junit.Test;

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
}
