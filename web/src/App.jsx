import { useEffect, useState } from "react";
import { onAuthStateChanged } from "firebase/auth";
import { auth } from "./firebase";
import Login from "./componentes/Login";
import Tablero from "./componentes/Tablero";

export default function App() {
  const [usuario, setUsuario] = useState(undefined);   // undefined = todavía no se sabe
  useEffect(() => onAuthStateChanged(auth, setUsuario), []);
  if (usuario === undefined) return null;
  return usuario ? <Tablero /> : <Login />;
}
