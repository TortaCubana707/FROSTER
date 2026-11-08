import { useCallback, useEffect, useRef, useState } from "react";
import { signOut } from "firebase/auth";
import { auth } from "../firebase";
import { ALERTAS, NIVELES, RUTAS } from "../contrato";
import { useNodo } from "../hooks/useNodo";
import { useLatido } from "../hooks/useLatido";
import { useComando } from "../hooks/useComando";
import { sonarAlarma } from "../sonido";
import Tarjeta from "./Tarjeta";
import Avisos from "./Avisos";

const grados = (v) => (typeof v === "number" ? `${v.toFixed(1)} °C` : "--");

export default function Tablero() {
  const lecturas = useNodo(RUTAS.lecturas) ?? {};
  const estado = useNodo(RUTAS.estado) ?? {};
  const ack = useNodo(RUTAS.ack);
  const vivo = useLatido(estado.uptime_s);
  const comando = useComando(ack);
  const [avisos, setAvisos] = useState([]);

  const avisar = useCallback((texto, tipo = "info") => {
    const id = Date.now() + Math.random();
    setAvisos((a) => [...a, { id, texto, tipo }]);
    setTimeout(() => setAvisos((a) => a.filter((x) => x.id !== id)), 4000);
  }, []);

  // Alarma: suena y avisa cuando aparece una alerta nueva
  const alertaPrevia = useRef(0);
  const alertaId = estado.alerta_id ?? 0;
  useEffect(() => {
    if (alertaId && alertaId !== alertaPrevia.current) {
      sonarAlarma();
      avisar(ALERTAS[alertaId]?.nombre ?? "Alerta", "grave");
    } else if (!alertaId && alertaPrevia.current) {
      avisar("Alerta resuelta", "ok");
    }
    alertaPrevia.current = alertaId;
  }, [alertaId, avisar]);

  const cambiarObjetivo = (delta) => {
    const nuevo = Math.min(40, Math.max(15, (estado.setpoint ?? 28) + delta));
    comando({ setpoint: nuevo })
      .then((ms) => avisar(`Objetivo ${nuevo} °C confirmado por el ESP32 en ${ms} ms`, "ok"))
      .catch((e) => avisar(e.message, "grave"));
  };

  return (
    <main>
      <header>
        <h1>FROST<span>ER</span> · tablero</h1>
        <span className={`insignia ${vivo ? "vivo" : "viejo"}`}>{vivo ? "EN VIVO" : "SIN DATOS DEL ESP32"}</span>
        <button onClick={() => signOut(auth)}>Salir</button>
      </header>

      {alertaId > 0 && <div className="alerta">⚠ {estado.alerta}</div>}

      <section className="rejilla">
        <Tarjeta titulo="Nivel" valor={`N${estado.nivel ?? "-"}`} detalle={NIVELES[estado.nivel]} />
        <Tarjeta titulo="Cámara" valor={grados(lecturas.ambiente)} detalle={`Humedad ${lecturas.humedad ?? "--"} %`} />
        <Tarjeta titulo="Objetivo" valor={grados(estado.setpoint)}>
          <div className="fila">
            <button onClick={() => cambiarObjetivo(-0.5)}>−</button>
            <button onClick={() => cambiarObjetivo(+0.5)}>+</button>
          </div>
        </Tarjeta>
        <Tarjeta titulo="Bloque frío" valor={grados(lecturas.placa_fria)} detalle={`Fuga ${lecturas.fuga_pct ?? "--"} %`} />
      </section>

      {/* E4: aquí siguen los demás indicadores, la gráfica, el modo, el historial de eventos... */}

      <Avisos avisos={avisos} />
    </main>
  );
}
