import { useCallback, useEffect, useRef } from "react";
import { ref, update } from "firebase/database";
import { db } from "../firebase";
import { RUTAS } from "../contrato";

const ESPERA_MAXIMA_MS = 5000;

/**
 * Manda un comando a config/ con cmd_id y espera a que el ESP32 lo confirme en ack/.
 * Devuelve una promesa con los ms de ida y vuelta, o un error si no se confirmó a tiempo.
 */
export function useComando(ack) {
  const pendientes = useRef(new Map());   // cmd_id -> { inicio, resolver, rechazar }

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
      rechazar(new Error("El ESP32 no confirmó el comando"));
    }, ESPERA_MAXIMA_MS);
    pendientes.current.set(id, { inicio: performance.now(), resolver, rechazar, reloj });
    update(ref(db, RUTAS.config), { ...cambios, cmd_id: id }).catch((e) => {
      pendientes.current.delete(id);
      clearTimeout(reloj);
      rechazar(e);
    });
  }), []);
}
