import { NIVELES } from "../contrato";
import Tarjeta from "./Tarjeta";

function Salida({ nombre, encendida, detalle }) {
  return (
    <div className={`salida ${encendida ? "on" : ""}`}>
      <span>{nombre}</span>
      <strong>{encendida ? detalle ?? "ON" : "OFF"}</strong>
    </div>
  );
}

// Nivel que decidió el ESP32, con el color del LED RGB (N1 verde, N2 cian, N3 azul) y sus salidas.
export default function Nivel({ estado }) {
  const nivel = estado.nivel ?? 0;
  return (
    <Tarjeta titulo="Nivel" className={`nivel n${nivel}`}>
      <div className="fila-base">
        <span className="valor grande">{nivel ? `N${nivel}` : "--"}</span>
        <span>{NIVELES[nivel] ?? "Sin dato"}</span>
      </div>
      <div className="tenue">{estado.motivo || " "}{estado.modo && estado.modo !== "auto" ? ` · modo ${estado.modo}` : ""}</div>
      <div className="salidas">
        <Salida nombre="Celda A" encendida={estado.celda_a} />
        <Salida nombre="Celda B" encendida={estado.celda_b} />
        <Salida nombre="Bomba" encendida={estado.bomba} />
        <Salida nombre="Ventiladores" encendida={estado.pwm_fans > 0} detalle={`${estado.pwm_fans} %`} />
      </div>
    </Tarjeta>
  );
}
