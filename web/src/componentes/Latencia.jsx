import { LATENCIA_OBJETIVO_MS } from "../contrato";
import Tarjeta from "./Tarjeta";

// Latencia de ida y vuelta de los comandos (escribir en config/ → ver el ack/ del ESP32)
// y datos de la placa (froster/dispositivo).
export default function Latencia({ latencias, enviando, medir, dispositivo }) {
  const ultima = latencias.at(-1);
  const promedio = latencias.length
    ? Math.round(latencias.reduce((a, b) => a + b, 0) / latencias.length)
    : null;
  const cumple = promedio !== null && promedio < LATENCIA_OBJETIVO_MS;
  return (
    <Tarjeta titulo="Comandos y ESP32" className="latencia">
      <div className="valor">{ultima !== undefined ? `${ultima} ms` : "--"}</div>
      <div className="tenue">
        {promedio !== null
          ? <>Promedio {promedio} ms en {latencias.length} comandos · <span className={cumple ? "bien" : "mal"}>{cumple ? "✓" : "✗"} meta &lt; {LATENCIA_OBJETIVO_MS} ms</span></>
          : "Ida y vuelta de un comando por Firebase"}
      </div>
      <button className="secundario" onClick={medir} disabled={enviando}>{enviando ? "Esperando al ESP32…" : "Medir latencia"}</button>
      <dl className="datos">
        <dt>IP</dt><dd>{dispositivo.ip ?? "--"}</dd>
        <dt>Wi-Fi</dt><dd>{typeof dispositivo.rssi === "number" ? `${dispositivo.rssi} dBm` : "--"}</dd>
        <dt>Firmware</dt><dd>{dispositivo.firmware ?? "--"}</dd>
        <dt>Caídas Wi-Fi</dt><dd>{dispositivo.caidas_wifi ?? "--"}</dd>
      </dl>
    </Tarjeta>
  );
}
