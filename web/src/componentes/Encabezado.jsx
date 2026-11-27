import { useState } from "react";
import { activarAudio, audioListo, notificacionesDisponibles, pedirPermisoNotificaciones, permisoNotificaciones } from "../sonido";

export default function Encabezado({ vivo, conectado, demo, alSalir, alSimularFuga }) {
  const [sonido, setSonido] = useState(audioListo());
  const [permiso, setPermiso] = useState(permisoNotificaciones());

  return (
    <header>
      <h1>FROST<span>ER</span> <small>panel de administración</small></h1>
      <span className={`insignia ${vivo ? "vivo" : "viejo"}`}>
        {vivo ? "● EN VIVO" : "○ SIN DATOS DEL ESP32"}
      </span>
      {conectado === false && <span className="insignia grave">SIN CONEXIÓN A FIREBASE</span>}
      {demo && <span className="insignia demo">DEMO · datos simulados, no es evidencia</span>}

      <div className="acciones">
        {demo && <button onClick={alSimularFuga}>Simular fuga</button>}
        {!sonido && (
          <button onClick={() => activarAudio().then(setSonido)} title="El navegador pide un clic antes de reproducir sonido">
            🔇 Activar sonido de alarmas
          </button>
        )}
        {notificacionesDisponibles() && permiso === "default" && (
          <button onClick={() => pedirPermisoNotificaciones().then(setPermiso)}>Activar notificaciones</button>
        )}
        <button onClick={alSalir}>Salir</button>
      </div>
    </header>
  );
}
