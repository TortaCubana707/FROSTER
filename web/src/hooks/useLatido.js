import { useEffect, useRef, useState } from "react";
import { LATIDO_MAXIMO_MS } from "../contrato";

/** true si el uptime del ESP32 cambió en los últimos 15 s; false si dejó de publicar. */
export function useLatido(uptime) {
  const anterior = useRef(null);
  const ultimo = useRef(0);
  const [, refrescar] = useState(0);

  useEffect(() => {
    if (typeof uptime !== "number") return;
    if (anterior.current !== null && uptime !== anterior.current) ultimo.current = Date.now();
    anterior.current = uptime;
  }, [uptime]);

  useEffect(() => {
    const t = setInterval(() => refrescar((n) => n + 1), 1000);
    return () => clearInterval(t);
  }, []);

  return ultimo.current > 0 && Date.now() - ultimo.current <= LATIDO_MAXIMO_MS;
}
