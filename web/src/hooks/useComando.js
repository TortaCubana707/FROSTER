import { useCallback, useEffect, useRef } from "react";
import { fuente } from "../datos";
import { RUTAS } from "../contrato";

const ESPERA_MAXIMA_MS = 5000;

/**
 * Manda un comando a config/ con cmd_id y espera a que el ESP32 lo confirme en ack/.
 * Devuelve una promesa con los ms de ida y vuelta, o un error si no se confirmó a tiempo.
 * Con cambios vacíos ({}) solo viaja el cmd_id: sirve para medir la latencia sin cambiar nada.
 */
export function useComando(ack) {
  const pendientes = useRef(new Map());   // cmd_id -> { inicio, resolver, rechazar, reloj }

  useEffect(() => {
    const p = ack && pendientes.current.get(ack.cmd_id);
    if (!p) return;
    pendientes.current.delete(ack.cmd_id);
    clearTimeout(p.reloj);
    p.resolver(Math.round(performance.now() - p.inicio));
  }, [ack]);

  return useCallback((cambios) => new Promise((resolver, rechazar) => {
    const id = Date.now();
    const reloj = setTimeout(() => {
      pendientes.current.delete(id);
      rechazar(new Error("El ESP32 no confirmó el comando en 5 s. ¿Está encendido y con Wi-Fi?"));
    }, ESPERA_MAXIMA_MS);
    pendientes.current.set(id, { inicio: performance.now(), resolver, rechazar, reloj });
    // origen: la app escribe "app"; así cada lado sabe quién hizo el último cambio (control cruzado)
    fuente.actualizar(RUTAS.config, { ...cambios, origen: "web", cmd_id: id }).catch((e) => {
      pendientes.current.delete(id);
      clearTimeout(reloj);
      rechazar(new Error(`No se pudo enviar: ${e.message}`));
    });
  }), []);
}
