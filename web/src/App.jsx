import { useEffect, useState } from "react";
import { fuente } from "./datos";
import Login from "./componentes/Login";
import Panel from "./componentes/Panel";

// El tablero es el panel de ADMINISTRACIÓN: solo entra una cuenta con roles/{uid} = "admin".
// La app Android es el nivel usuario. Las reglas de Firebase hacen cumplir lo mismo del lado del servidor.
export default function App() {
  const [usuario, setUsuario] = useState(undefined);   // undefined = todavía no se sabe
  const [rol, setRol] = useState(undefined);
  useEffect(() => fuente.alCambiarSesion(setUsuario), []);
  useEffect(() => {
    if (!usuario) { setRol(undefined); return undefined; }
    return fuente.rol(usuario.uid, setRol);
  }, [usuario]);

  if (usuario === undefined || (usuario && rol === undefined)) return <p className="cargando">Cargando…</p>;
  if (!usuario) return <Login />;
  if (rol !== "admin") {
    return (
      <div className="tarjeta acceso">
        <h1>FROST<span>ER</span></h1>
        <p>La cuenta <b>{usuario.email}</b> no es administradora.</p>
        <p className="tenue">Este tablero es el panel de administración. Para ver el sistema como usuario usa la app
          Android. Si debes ser admin, el dueño del proyecto agrega <code>roles/{usuario.uid}: "admin"</code> en Firebase.</p>
        <button className="primario" onClick={() => fuente.salir()}>Salir</button>
      </div>
    );
  }
  return <Panel />;
}
