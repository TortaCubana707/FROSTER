import { useState } from "react";
import { fuente } from "../datos";

// Mensajes en español para los errores más comunes de Firebase Authentication
const ERRORES = {
  "auth/invalid-credential": "Correo o contraseña incorrectos.",
  "auth/invalid-email": "El correo no tiene un formato válido.",
  "auth/too-many-requests": "Demasiados intentos. Espera unos minutos y vuelve a probar.",
  "auth/network-request-failed": "Sin conexión a internet.",
};

export default function Login() {
  const [correo, setCorreo] = useState("");
  const [clave, setClave] = useState("");
  const [error, setError] = useState("");
  const [entrando, setEntrando] = useState(false);

  const entrar = (e) => {
    e.preventDefault();
    setError("");
    setEntrando(true);
    fuente.entrar(correo.trim(), clave).catch((err) => {
      setError(ERRORES[err.code] ?? `No se pudo entrar (${err.code ?? err.message}).`);
      setEntrando(false);
    });
  };

  return (
    <form className="tarjeta acceso" onSubmit={entrar}>
      <h1>FROST<span>ER</span></h1>
      <p className="tenue">Panel de administración del módulo de enfriamiento. Solo para cuentas con rol de administrador.</p>
      <label>Correo
        <input type="email" required value={correo} onChange={(e) => setCorreo(e.target.value)} autoComplete="username" />
      </label>
      <label>Contraseña
        <input type="password" required value={clave} onChange={(e) => setClave(e.target.value)} autoComplete="current-password" />
      </label>
      <button className="primario" disabled={entrando}>{entrando ? "Entrando…" : "Entrar"}</button>
      {error && <p className="error" role="alert">{error}</p>}
      <p className="tenue pequeno">¿Sin el prototipo a la mano? <a href="?demo">Ver con datos simulados</a></p>
    </form>
  );
}
