import { useCallback, useState } from "react";

/** Mensajes emergentes (tipo toast) que se cierran solos o con la ×. */
export function useAvisos() {
  const [avisos, setAvisos] = useState([]);

  const cerrar = useCallback((id) => setAvisos((a) => a.filter((x) => x.id !== id)), []);

  const avisar = useCallback((texto, tipo = "info", duracionMs = 5000) => {
    const id = Date.now() + Math.random();
    setAvisos((a) => [...a.slice(-3), { id, texto, tipo }]);   // máximo 4 a la vez
    setTimeout(() => cerrar(id), duracionMs);
  }, [cerrar]);

  return { avisos, avisar, cerrar };
}
