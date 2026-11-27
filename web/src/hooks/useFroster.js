import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from "react";
import { RUTAS } from "../contrato";
import { fuente } from "../datos";
import { useConexion, useEventos, useNodo } from "./useNodo";
import { useLatido } from "./useLatido";
import { useComando } from "./useComando";
import { useHistorial } from "./useHistorial";
import { diagnosticar } from "../diagnostico";

/**
 * Todos los datos del sistema en un solo lugar. Cada ventana del panel los toma de aquí con
 * useFroster(); así una ventana nueva no abre otra conexión a Firebase.
 */
export const FrosterContexto = createContext(null);
export const useFroster = () => useContext(FrosterContexto);

export function useDatosFroster(avisar) {
  const lecturasNodo = useNodo(RUTAS.lecturas);
  const lecturas = lecturasNodo ?? {};
  const estado = useNodo(RUTAS.estado) ?? {};
  const config = useNodo(RUTAS.config) ?? {};
  const dispositivo = useNodo(RUTAS.dispositivo) ?? {};
  const ack = useNodo(RUTAS.ack);
  const eventos = useEventos(40);
  const conectado = useConexion();
  const vivo = useLatido(estado.uptime_s);
  const comando = useComando(ack);
  const puntos = useHistorial(lecturasNodo, estado.setpoint);

  const [latencias, setLatencias] = useState([]);
  const [enviando, setEnviando] = useState(false);

  // Toda orden pasa por aquí: config/ con cmd_id → el ESP32 la aplica → ack/ → aviso con los ms
  const enviar = useCallback((cambios, queSePidio) => {
    setEnviando(true);
    return comando(cambios)
      .then((ms) => {
        setLatencias((l) => [...l.slice(-19), ms]);
        avisar(`${queSePidio} · confirmado por el ESP32 en ${ms} ms`, "ok");
        return ms;
      })
      .catch((e) => { avisar(e.message, "grave", 7000); })
      .finally(() => setEnviando(false));
  }, [comando, avisar]);

  /** Orden de una sola vez: reiniciar, pruebas, poner contadores en cero. */
  const accion = useCallback((nombre, texto) =>
    enviar({ accion: nombre, accion_id: Date.now() }, texto), [enviar]);

  /**
   * Un ajuste fino. Se manda el objeto ajustes COMPLETO (lo que el ESP32 dice tener aplicado en
   * estado/ajustes + el cambio): el stream del ESP32 lo recibe como un solo objeto y lo valida entero.
   */
  const aplicados = estado.ajustes;
  const ajustar = useCallback((clave, valor, texto) =>
    enviar({ ajustes: { ...(aplicados ?? {}), [clave]: valor } }, texto), [enviar, aplicados]);

  // Control cruzado: si la orden vino de la app (config.origen = "app"), el panel lo avisa
  const previo = useRef(null);
  useEffect(() => {
    const antes = previo.current;
    previo.current = config;
    if (!antes || config.origen !== "app" || config.cmd_id === antes.cmd_id) return;
    const partes = [];
    if (config.setpoint !== antes.setpoint && config.setpoint != null) partes.push(`objetivo a ${config.setpoint} °C`);
    if (config.modo !== antes.modo && config.modo) partes.push(`modo a ${config.modo === "off" ? "apagado" : config.modo}`);
    avisar(`Desde la app móvil: ${partes.length ? partes.join(" y ") : "nueva orden"}`, "info", 6000);
  }, [config, avisar]);

  const diagnostico = useMemo(
    () => diagnosticar({ lecturas, estado, dispositivo, vivo, conectado }),
    [lecturas, estado, dispositivo, vivo, conectado]
  );

  return {
    lecturas, estado, config, dispositivo, eventos, conectado, vivo, puntos,
    latencias, enviando, enviar, accion, ajustar, diagnostico, demo: fuente.demo,
  };
}
