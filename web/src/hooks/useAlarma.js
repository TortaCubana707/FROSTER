import { useEffect, useRef, useState } from "react";
import { ALERTAS } from "../contrato";
import { notificar, sonarAlarma, sonarResuelta } from "../sonido";

const REPETIR_CADA_MS = 15000;

/**
 * Alarma de alertas críticas: al aparecer una alerta nueva suena, manda un aviso en pantalla y una
 * notificación del sistema, y cambia el título de la pestaña. Mientras siga activa vuelve a sonar
 * cada 15 s hasta que alguien la silencia. Al resolverse avisa y suena un tono corto.
 */
export function useAlarma(alertaId, avisar) {
  const anterior = useRef(0);
  const [silenciada, setSilenciada] = useState(false);

  useEffect(() => {
    const previa = anterior.current;
    anterior.current = alertaId;
    if (alertaId && alertaId !== previa) {
      const { nombre, queHacer } = ALERTAS[alertaId] ?? { nombre: "Alerta", queHacer: "" };
      setSilenciada(false);
      sonarAlarma(alertaId);
      avisar(`Alerta: ${nombre}`, "grave", 8000);
      notificar(`FROSTER · ${nombre}`, queHacer);
    } else if (!alertaId && previa) {
      setSilenciada(false);
      sonarResuelta();
      avisar("Alerta resuelta: el ESP32 volvió a la normalidad", "ok");
    }
  }, [alertaId, avisar]);

  // Repetición mientras la alerta siga activa y nadie la haya silenciado
  useEffect(() => {
    if (!alertaId || silenciada) return undefined;
    const t = setInterval(() => sonarAlarma(alertaId), REPETIR_CADA_MS);
    return () => clearInterval(t);
  }, [alertaId, silenciada]);

  // Título de la pestaña: se ve aunque la ventana esté detrás de otra
  useEffect(() => {
    document.title = alertaId ? `⚠ ALERTA · FROSTER` : "FROSTER · Tablero";
  }, [alertaId]);

  return { silenciada, silenciar: () => setSilenciada(true) };
}
