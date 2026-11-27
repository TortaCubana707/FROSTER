import { useEffect, useState } from "react";
import { fuente } from "../datos";

/** Valor en vivo de una ruta. `null` mientras llega el primer dato. */
export function useNodo(ruta) {
  const [valor, setValor] = useState(null);
  useEffect(() => fuente.escuchar(ruta, (v) => setValor(v ?? {})), [ruta]);
  return valor;
}

/** Los últimos `n` eventos (froster/eventos), el más reciente primero. */
export function useEventos(n) {
  const [eventos, setEventos] = useState(null);
  useEffect(
    () => fuente.escuchar("froster/eventos", (lista) => setEventos([...lista].reverse()), { ultimos: n }),
    [n]
  );
  return eventos;
}

/** true si este navegador está conectado a Firebase (.info/connected). */
export function useConexion() {
  const [conectado, setConectado] = useState(null);
  useEffect(() => fuente.escuchar(".info/connected", (v) => setConectado(v === true)), []);
  return conectado;
}
