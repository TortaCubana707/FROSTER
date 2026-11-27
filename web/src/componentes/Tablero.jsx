import { useCallback, useState } from "react";
import { RUTAS } from "../contrato";
import { fuente } from "../datos";
import { useConexion, useEventos, useNodo } from "../hooks/useNodo";
import { useLatido } from "../hooks/useLatido";
import { useComando } from "../hooks/useComando";
import { useHistorial } from "../hooks/useHistorial";
import { useAvisos } from "../hooks/useAvisos";
import { useAlarma } from "../hooks/useAlarma";
import Encabezado from "./Encabezado";
import BannerAlerta from "./BannerAlerta";
import Nivel from "./Nivel";
import Tarjeta from "./Tarjeta";
import Grafica from "./Grafica";
import Eventos from "./Eventos";
import Latencia from "./Latencia";
import { Modo, Objetivo } from "./Controles";
import Avisos from "./Avisos";

const grados = (v) => (typeof v === "number" ? `${v.toFixed(1)} °C` : "--");
const signo = (v) => (v > 0 ? "+" : "");

export default function Tablero() {
  const lecturasNodo = useNodo(RUTAS.lecturas);
  const lecturas = lecturasNodo ?? {};
  const estado = useNodo(RUTAS.estado) ?? {};
  const config = useNodo(RUTAS.config) ?? {};
  const dispositivo = useNodo(RUTAS.dispositivo) ?? {};
  const ack = useNodo(RUTAS.ack);
  const eventos = useEventos(15);
  const conectado = useConexion();
  const vivo = useLatido(estado.uptime_s);
  const comando = useComando(ack);
  const puntos = useHistorial(lecturasNodo, estado.setpoint);
  const { avisos, avisar, cerrar } = useAvisos();
  const alertaId = estado.alerta_id ?? 0;
  const { silenciada, silenciar } = useAlarma(alertaId, avisar);

  const [latencias, setLatencias] = useState([]);
  const [enviando, setEnviando] = useState(false);

  // Todas las órdenes pasan por aquí: config/ con cmd_id → ack/ del ESP32 → aviso con los ms
  const enviar = useCallback((cambios, queSePidio) => {
    setEnviando(true);
    comando(cambios)
      .then((ms) => {
        setLatencias((l) => [...l.slice(-19), ms]);
        avisar(`${queSePidio} · confirmado por el ESP32 en ${ms} ms`, "ok");
      })
      .catch((e) => avisar(e.message, "grave", 7000))
      .finally(() => setEnviando(false));
  }, [comando, avisar]);

  return (
    <main>
      <Encabezado vivo={vivo} conectado={conectado} demo={fuente.demo}
        alSalir={() => fuente.salir()} alSimularFuga={() => fuente.simularFuga?.()} />

      <BannerAlerta alertaId={alertaId} textoEsp32={estado.alerta} silenciada={silenciada} alSilenciar={silenciar} />

      {!vivo && (
        <p className="nota">
          Los valores son los últimos que publicó el ESP32. Si sigue sin datos, revisa que la placa esté encendida
          y con el firmware de la rama <code>e2-firebase</code>.
        </p>
      )}

      <div className="rejilla">
        <Nivel estado={estado} />

        <Tarjeta titulo="Cámara" valor={grados(lecturas.ambiente)}
          detalle={`Humedad ${lecturas.humedad ?? "--"} %`}>
          {typeof lecturas.pendiente === "number" && (
            <div className="tenue">
              {signo(lecturas.pendiente)}{lecturas.pendiente.toFixed(2)} °C/min · prevista {grados(lecturas.prediccion)} en 2 min
            </div>
          )}
        </Tarjeta>

        <Tarjeta titulo="Bloque frío" valor={grados(lecturas.placa_fria)}
          detalle={`Rocío ${grados(lecturas.punto_rocio)} · fuga ${lecturas.fuga_pct ?? "--"} %`}>
          {estado.condensa && <div className="chip ambar">Bajo el punto de rocío: puede condensar</div>}
        </Tarjeta>

        <Grafica puntos={puntos} />
        <Eventos eventos={eventos} />

        <Objetivo setpoint={estado.setpoint} enviando={enviando} enviar={enviar} avisar={avisar} />
        <Modo estado={estado} config={config} enviando={enviando} enviar={enviar} />
        <Latencia latencias={latencias} enviando={enviando} medir={() => enviar({}, "Prueba de latencia")}
          dispositivo={dispositivo} />
      </div>

      <footer className="tenue">
        ESP32 → Wi-Fi → Firebase (<code>froster/</code>) → este panel (administrador) y la app Android (usuario).
      </footer>

      <Avisos avisos={avisos} cerrar={cerrar} />
    </main>
  );
}
