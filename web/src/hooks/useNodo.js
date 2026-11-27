import { useEffect, useState } from "react";
import { ref, onValue } from "firebase/database";
import { db } from "../firebase";

/** Valor en vivo de una ruta de Firebase. `null` mientras llega el primer dato. */
export function useNodo(ruta) {
  const [valor, setValor] = useState(null);
  useEffect(() => onValue(ref(db, ruta), (s) => setValor(s.val() ?? {})), [ruta]);
  return valor;
}
