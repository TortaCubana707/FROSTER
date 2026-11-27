import { MODOS, SETPOINT_MAX, SETPOINT_MIN, ajustarSetpoint } from "../contrato";
import Interruptor from "./Interruptor";
import Tarjeta from "./Tarjeta";

const grados = (v) => (typeof v === "number" ? `${v.toFixed(1)} °C` : "--");

/** Objetivo de la cámara (umbral dinámico del Proyecto 1). */
export function Objetivo({ setpoint, enviando, enviar, avisar }) {
  const cambiar = (delta) => {
    const actual = setpoint ?? 28;
    const nuevo = ajustarSetpoint(actual + delta);
    if (nuevo === actual) {
      avisar(`El objetivo ya está en su límite (${SETPOINT_MIN}–${SETPOINT_MAX} °C)`, "info");
      return;
    }
    enviar({ setpoint: nuevo }, `Objetivo ${nuevo.toFixed(1)} °C`);
  };
  return (
    <Tarjeta titulo="Objetivo">
      <div className="objetivo">
        <button onClick={() => cambiar(-0.5)} disabled={enviando} aria-label="Bajar el objetivo medio grado">−</button>
        <span className="valor">{grados(setpoint)}</span>
        <button onClick={() => cambiar(+0.5)} disabled={enviando} aria-label="Subir el objetivo medio grado">+</button>
      </div>
      <div className="tenue">Nivel 2 arriba del objetivo · nivel 3 a +2.5 °C</div>
    </Tarjeta>
  );
}

/** Modo de trabajo, celdas en manual y aviso de carga alta. */
export function Modo({ estado, config, enviando, enviar }) {
  const manual = estado.modo === "manual";
  return (
    <Tarjeta titulo="Modo y celdas" className="modo">
      <div className="segmentado" role="group" aria-label="Modo de trabajo">
        {MODOS.map((m) => (
          <button
            key={m.valor}
            aria-pressed={estado.modo === m.valor}
            disabled={enviando}
            onClick={() => estado.modo !== m.valor && enviar({ modo: m.valor }, `Modo ${m.nombre.toLowerCase()}`)}
          >
            {m.nombre}
          </button>
        ))}
      </div>
      <Interruptor texto="Celda A" encendido={config.manual_a} deshabilitado={!manual || enviando}
        ayuda="Las celdas solo se controlan a mano en modo manual"
        alCambiar={(v) => enviar({ manual_a: v }, `Celda A ${v ? "ON" : "OFF"}`)} />
      <Interruptor texto="Celda B" encendido={config.manual_b} deshabilitado={!manual || enviando}
        ayuda="Las celdas solo se controlan a mano en modo manual"
        alCambiar={(v) => enviar({ manual_b: v }, `Celda B ${v ? "ON" : "OFF"}`)} />
      <Interruptor texto="Aviso de carga alta" encendido={config.carga_alta} deshabilitado={enviando}
        alCambiar={(v) => enviar({ carga_alta: v }, `Aviso de carga alta ${v ? "activado" : "desactivado"}`)} />
    </Tarjeta>
  );
}
