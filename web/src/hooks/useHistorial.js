import { useEffect, useRef, useState } from "react";
import { fuente } from "../datos";

const VENTANA_MS = 30 * 60000;          // la gráfica muestra los últimos 30 minutos
const CLAVE = "froster.grafica.v1";

function cargar() {
  const inicial = fuente.historialInicial();
  if (inicial) return inicial;
  try {
    const guardado = JSON.parse(localStorage.getItem(CLAVE) ?? "[]");
    return Array.isArray(guardado) ? guardado.filter((p) => Date.now() - p.t <= VENTANA_MS) : [];
  } catch {
    return [];   // el navegador puede bloquear el almacenamiento: la gráfica empieza vacía
  }
}

/**
 * Junta un punto por cada lectura nueva del ESP32 (cámara, bloque y objetivo).
 * Firebase guarda solo la lectura actual; la historia de 30 min vive en este navegador.
 */
export function useHistorial(lecturas, objetivo) {
  const [puntos, setPuntos] = useState(cargar);
  const ultimoTs = useRef(null);

  useEffect(() => {
    if (!lecturas || typeof lecturas.ambiente !== "number") return;
    if (lecturas.ts && lecturas.ts === ultimoTs.current) return;
    ultimoTs.current = lecturas.ts ?? null;
    // Hora del navegador: evita saltos si el reloj del ESP32 aún no sincroniza por NTP.
    const punto = {
      t: Date.now(),
      camara: lecturas.ambiente,
      bloque: typeof lecturas.placa_fria === "number" ? lecturas.placa_fria : null,
      objetivo: typeof objetivo === "number" ? objetivo : null,
    };
    setPuntos((anteriores) => [...anteriores, punto].filter((p) => punto.t - p.t <= VENTANA_MS));
  }, [lecturas, objetivo]);

  useEffect(() => {
    if (fuente.demo) return;
    try { localStorage.setItem(CLAVE, JSON.stringify(puntos)); } catch { /* sin almacenamiento */ }
  }, [puntos]);

  return puntos;
}
