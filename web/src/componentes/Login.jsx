import { useState } from "react";
import { signInWithEmailAndPassword } from "firebase/auth";
import { auth } from "../firebase";

export default function Login() {
  const [correo, setCorreo] = useState("");
  const [clave, setClave] = useState("");
  const [error, setError] = useState("");

  const entrar = (e) => {
    e.preventDefault();
    setError("");
    signInWithEmailAndPassword(auth, correo.trim(), clave)
      .catch((err) => setError("No se pudo entrar: " + err.code));
  };

  return (
    <form className="tarjeta acceso" onSubmit={entrar}>
      <h1>FROST<span>ER</span></h1>
      <p className="tenue">Tablero en vivo. Entra con la cuenta de Firebase del proyecto.</p>
      <label>Correo<input type="email" value={correo} onChange={(e) => setCorreo(e.target.value)} autoComplete="username" /></label>
      <label>Contraseña<input type="password" value={clave} onChange={(e) => setClave(e.target.value)} autoComplete="current-password" /></label>
      <button className="primario">Entrar</button>
      {error && <p className="error">{error}</p>}
    </form>
  );
}
