import { useEffect, useMemo, useState } from "react";
import { ACCIONES, AJUSTES, ALERTAS, TIPOS_EVENTO, ajuste } from "../contrato";
import { useFroster } from "../hooks/useFroster";
import { GRAVEDAD } from "../diagnostico";
import Interruptor from "../componentes/Interruptor";
import Nivel from "../componentes/Nivel";
import Grafica from "../componentes/Grafica";
import Latencia from "../componentes/Latencia";
import { Modo, Objetivo } from "../componentes/Controles";

const num = (v) => typeof v === "number" && !Number.isNaN(v);
const grados = (v) => (num(v) ? `${v.toFixed(1)} °C` : "--");
const ETIQUETA = { falla: "FALLA", aviso: "REVISAR", activo: "ACTIVO", ok: "OK", desconocido: "SIN DATO", sin_sensor: "NO INSTALADO" };

/** Fila etiqueta / valor. */
const Dato = ({ k, v, clase = "" }) => (<><dt>{k}</dt><dd className={clase}>{v}</dd></>);

/** Barra horizontal 0–100 con marca de umbral opcional. */
function Barra({ valor, umbral, peligro }) {
  const v = num(valor) ? Math.max(0, Math.min(100, valor)) : 0;
  return (
    <div className="barra" role="meter" aria-valuemin={0} aria-valuemax={100} aria-valuenow={v}>
      <div className={`relleno ${peligro ? "mal" : ""}`} style={{ width: `${v}%` }} />
      {num(umbral) && <div className="umbral" style={{ left: `${umbral}%` }} title={`Umbral ${umbral} %`} />}
    </div>
  );
}

/** Un ajuste fino del ESP32: valor aplicado (estado/ajustes) + control para cambiarlo. */
function ControlAjuste({ clave, validar }) {
  const { estado, ajustar, enviando } = useFroster();
  const a = ajuste(clave);
  const aplicado = estado.ajustes?.[clave];
  const [valor, setValor] = useState(aplicado ?? a.fabrica);
  useEffect(() => { if (num(aplicado)) setValor(aplicado); }, [aplicado]);
  const cambio = num(aplicado) && Number(valor) !== aplicado;
  const error = cambio && validar ? validar(Number(valor)) : "";
  const enviarValor = (v) => ajustar(clave, v, `${a.nombre}: ${v} ${a.unidad}`);
  return (
    <div className="ajuste">
      <div className="ajuste-cabeza">
        <label htmlFor={`aj-${clave}`}>{a.nombre}</label>
        <span className="aplicado" title="Valor que el ESP32 tiene aplicado">{num(aplicado) ? `${aplicado} ${a.unidad}` : "--"}</span>
      </div>
      <div className="ajuste-control">
        <input id={`aj-${clave}`} type="range" min={a.min} max={a.max} step={a.paso} value={valor}
          onChange={(e) => setValor(Number(e.target.value))} disabled={!num(aplicado)} />
        <output>{valor} {a.unidad}</output>
        <button disabled={!cambio || !!error || enviando} onClick={() => enviarValor(Number(valor))}>Aplicar</button>
      </div>
      {error ? <div className="mal pequeno">{error}</div> : a.ayuda && <div className="tenue pequeno">{a.ayuda}</div>}
      {num(aplicado) && aplicado !== a.fabrica && (
        <button className="enlace pequeno" disabled={enviando} onClick={() => enviarValor(a.fabrica)}>
          Volver al de fábrica ({a.fabrica} {a.unidad})
        </button>
      )}
    </div>
  );
}

function BotonAccion({ nombre, confirmar, clase = "" }) {
  const { accion, enviando, vivo } = useFroster();
  return (
    <button className={clase} disabled={enviando || !vivo}
      onClick={() => (!confirmar || window.confirm(confirmar)) && accion(nombre, ACCIONES[nombre])}>
      {ACCIONES[nombre]}
    </button>
  );
}

// ------------------------------------------------------------------------------------------------
export function Resumen() {
  const { estado, diagnostico } = useFroster();
  const alerta = ALERTAS[estado.alerta_id ?? 0];
  return (
    <>
      <Nivel estado={estado} />
      <div className={`salud est-${diagnostico.general}`}>
        {diagnostico.general === "ok" && "Todo en orden"}
        {diagnostico.general === "aviso" && `${diagnostico.avisos.length} cosa(s) por revisar`}
        {diagnostico.general === "falla" && `${diagnostico.fallas.length} falla(s): ${diagnostico.fallas.map((f) => f.nombre).join(", ")}`}
        {diagnostico.general === "desconocido" && "Sin datos actuales del ESP32"}
      </div>
      {estado.alerta_id > 0 && <div className="mal pequeno">{alerta?.queHacer}</div>}
    </>
  );
}

export function Temperaturas() {
  const { lecturas: l, estado: e } = useFroster();
  const s = num(l.pendiente) && l.pendiente > 0 ? "+" : "";
  return (
    <dl className="datos amplio">
      <Dato k="Cámara" v={grados(l.ambiente)} />
      <Dato k="Objetivo" v={grados(e.setpoint)} />
      <Dato k="Tendencia" v={num(l.pendiente) ? `${s}${l.pendiente.toFixed(2)} °C/min` : "--"} />
      <Dato k="Prevista en 2 min" v={grados(l.prediccion)} />
      <Dato k="Humedad" v={num(l.humedad) ? `${l.humedad.toFixed(0)} %` : "--"} />
      <Dato k="Punto de rocío" v={grados(l.punto_rocio)} />
      <Dato k="Bloque frío" v={grados(l.placa_fria)} clase={e.congelado ? "mal" : ""} />
      <Dato k="Condensación" v={e.condensa ? "Sí: bajo el rocío" : "No"} clase={e.condensa ? "ambar" : ""} />
      <Dato k="Agua (waterblock)" v={num(l.agua) ? grados(l.agua) : "sin sensor"} />
    </dl>
  );
}

export function GraficaVentana() {
  const { puntos } = useFroster();
  return <Grafica puntos={puntos} />;
}

export function Control() {
  const { estado, config, enviando, enviar, avisar } = useFroster();
  return (
    <div className="control-doble">
      <Objetivo setpoint={estado.setpoint} enviando={enviando} enviar={enviar} avisar={avisar} />
      <Modo estado={estado} config={config} enviando={enviando} enviar={enviar} />
      <p className="tenue pequeno">
        Último cambio desde: <b>{config.origen === "app" ? "la app móvil" : config.origen === "web" ? "este panel" : "--"}</b>.
        La app también puede cambiar el objetivo y el modo (control cruzado).
      </p>
    </div>
  );
}

export function Ventilacion() {
  const { estado: e, config, enviar, enviando } = useFroster();
  const [pct, setPct] = useState(config.pwm_pct ?? 70);
  useEffect(() => { if (num(config.pwm_pct)) setPct(config.pwm_pct); }, [config.pwm_pct]);
  const manual = e.pwm_auto === false;
  const celdas = e.celda_a || e.celda_b;
  return (
    <>
      <div className="fila-base">
        <span className="valor grande">{num(e.pwm_fans) ? `${e.pwm_fans} %` : "--"}</span>
        <span>{e.bomba ? `bomba y ventiladores · ${e.pwm_hz ?? "--"} Hz` : "apagados"}</span>
      </div>
      <Barra valor={e.pwm_fans} />
      <p className="tenue pequeno">La bomba y los 3 ventiladores comparten un MOSFET (GPIO27): se mueven juntos.
        No hay tacómetro, así que el % es la orden enviada, no RPM medidas.</p>
      <Interruptor texto="Control manual del PWM" encendido={manual} deshabilitado={enviando}
        alCambiar={(v) => enviar({ pwm_auto: !v }, v ? "PWM en manual" : "PWM por nivel (auto)")} />
      {manual && (
        <div className="ajuste">
          <div className="ajuste-control">
            <input type="range" min="20" max="100" step="5" value={pct} onChange={(ev) => setPct(Number(ev.target.value))} aria-label="PWM manual" />
            <output>{pct} %</output>
            <button disabled={enviando || pct === config.pwm_pct} onClick={() => enviar({ pwm_pct: pct }, `PWM manual ${pct} %`)}>Aplicar</button>
          </div>
          {celdas && pct < 50 && <div className="ambar pequeno">Con una celda encendida el ESP32 no baja de 50 %: sin flujo se sobrecalienta.</div>}
        </div>
      )}
      <details>
        <summary>PWM por nivel y frecuencia</summary>
        {["pwm_n1", "pwm_n2", "pwm_n3", "pwm_hz"].map((c) => <ControlAjuste key={c} clave={c} />)}
      </details>
    </>
  );
}

export function Celdas() {
  const { estado: e } = useFroster();
  const total = (e.horas_a ?? 0) + (e.horas_b ?? 0);
  const reparto = total > 0 ? Math.round(((e.horas_a ?? 0) / total) * 100) : 50;
  return (
    <>
      <div className="celdas">
        {["a", "b"].map((x) => (
          <div key={x} className={`celda-tarjeta ${e[`celda_${x}`] ? "on" : ""}`}>
            <b>Celda {x.toUpperCase()}</b>
            <span className="valor">{e[`celda_${x}`] ? "ON" : "OFF"}</span>
            <span className="tenue">{num(e[`horas_${x}`]) ? e[`horas_${x}`].toFixed(1) : "--"} h de uso</span>
            {e.prueba === `celda_${x}` && <span className="chip ambar">EN PRUEBA</span>}
          </div>
        ))}
      </div>
      <div className="tenue pequeno">Reparto del desgaste: A {reparto} % · B {100 - reparto} %. En nivel 2 trabaja la que lleva menos horas.</div>
      <Barra valor={reparto} umbral={50} />
      <div className="botones">
        <BotonAccion nombre="probar_celda_a" />
        <BotonAccion nombre="probar_celda_b" />
        <BotonAccion nombre="reset_horas" confirmar="¿Poner en cero las horas de uso de las dos celdas?" />
      </div>
    </>
  );
}

export function Fugas() {
  const { lecturas: l, estado: e } = useFroster();
  const umbral = e.ajustes?.fuga_umbral;
  const estadoFuga = e.fuga ? "FUGA: todo cortado" : num(l.fuga_pct) && num(umbral) && l.fuga_pct >= umbral * 0.5 ? "Húmedo" : "Seco";
  return (
    <>
      <div className="fila-base">
        <span className={`valor grande ${e.fuga ? "mal" : ""}`}>{num(l.fuga_pct) ? `${l.fuga_pct} %` : "--"}</span>
        <span className={e.fuga ? "mal" : ""}>{estadoFuga}</span>
      </div>
      <Barra valor={l.fuga_pct} umbral={umbral} peligro={e.fuga} />
      <dl className="datos">
        <Dato k="Lectura ADC" v={l.fuga_raw ?? "--"} />
        <Dato k="Calibración" v="seco 4095 · mojado 1650" />
        <Dato k="Confirmación" v="0.3 s mojado → corta" />
        <Dato k="Liberación" v="30 s seco" />
      </dl>
      <ControlAjuste clave="fuga_umbral" />
    </>
  );
}

export function Protecciones() {
  const { estado: e } = useFroster();
  const aj = e.ajustes ?? {};
  const orden = (clave) => (v) => {
    const x = { ...aj, [clave]: v };
    return x.congela_corte < x.congela_aviso && x.congela_aviso < x.congela_rearme ? "" : "Debe cumplirse: corte < aviso < rearme.";
  };
  return (
    <>
      <h3 className="sub">Congelamiento del bloque</h3>
      {["congela_aviso", "congela_corte", "congela_rearme"].map((c) => <ControlAjuste key={c} clave={c} validar={orden(c)} />)}
      <h3 className="sub">Tiempos de las celdas y la bomba</h3>
      {AJUSTES.filter((a) => a.grupo === "tiempos").map((a) => <ControlAjuste key={a.clave} clave={a.clave} />)}
    </>
  );
}

export function Sistema() {
  const { estado: e, enviar, enviando } = useFroster();
  return (
    <>
      <Interruptor texto="Silenciar el buzzer del prototipo" encendido={!!e.silencio} deshabilitado={enviando}
        alCambiar={(v) => enviar({ silencio: v }, v ? "Buzzer silenciado" : "Buzzer activo")} />
      <p className="tenue pequeno">Silencia el buzzer del ESP32 (las alertas siguen en el LED, la LCD y este panel).</p>
      <h3 className="sub">Calibración de sensores</h3>
      {AJUSTES.filter((a) => a.grupo === "cal").map((a) => <ControlAjuste key={a.clave} clave={a.clave} />)}
    </>
  );
}

export function Diagnostico() {
  const { diagnostico } = useFroster();
  const lista = useMemo(
    () => [...diagnostico.componentes].sort((a, b) => GRAVEDAD[b.estado] - GRAVEDAD[a.estado]),
    [diagnostico]
  );
  return (
    <ul className="diagnostico">
      {lista.map((c) => (
        <li key={c.id} className={`est-${c.estado}`}>
          <span className="semaforo" aria-hidden="true" />
          <div>
            <b>{c.nombre}</b> <span className="tenue">· {c.valor}</span>
            <div className="tenue pequeno">{c.detalle}</div>
            {c.queHacer && <div className="que-hacer">{c.queHacer}</div>}
          </div>
          <span className="estado-txt">{ETIQUETA[c.estado]}</span>
        </li>
      ))}
    </ul>
  );
}

export function Pruebas() {
  const { estado: e } = useFroster();
  return (
    <>
      <p className="tenue pequeno">Cada prueba enciende una sola pieza unos segundos para comprobar que responde.
        Las celdas solo se prueban con la bomba encendida, el bloque arriba del aviso de congelamiento y sin fuga.</p>
      <div className="botones columna">
        {["probar_bomba", "probar_celda_a", "probar_celda_b", "probar_buzzer", "probar_led"].map((a) => <BotonAccion key={a} nombre={a} />)}
      </div>
      <div className="resultado-prueba">
        {e.prueba ? <span className="chip ambar">Probando: {e.prueba}</span> : <span className="tenue">Sin prueba en curso</span>}
        {e.prueba_resultado && <div>{e.prueba_resultado}</div>}
      </div>
    </>
  );
}

export function Registro() {
  const { eventos } = useFroster();
  const [tipo, setTipo] = useState("todos");
  const hora = (t) => (t ? new Date(t).toLocaleString("es-MX", { hour: "2-digit", minute: "2-digit", second: "2-digit", day: "2-digit", month: "short" }) : "--");
  const lista = (eventos ?? []).filter((e) => tipo === "todos" || e.tipo === tipo);
  return (
    <>
      <div className="segmentado chico" role="group" aria-label="Filtrar eventos">
        {["todos", ...Object.keys(TIPOS_EVENTO)].map((t) => (
          <button key={t} aria-pressed={tipo === t} onClick={() => setTipo(t)}>{t === "todos" ? "Todos" : TIPOS_EVENTO[t]}</button>
        ))}
      </div>
      {eventos === null && <p className="tenue">Cargando…</p>}
      {eventos && lista.length === 0 && <p className="tenue">Sin eventos de este tipo.</p>}
      <ol className="registro">
        {lista.map((e) => (
          <li key={e.id} className={`evento ${e.tipo}`}>
            <span className="hora">{hora(e.ts)}</span>
            <span className="texto">{e.texto}</span>
            <span className="tenue">N{e.nivel ?? "-"}{num(e.camara) ? ` · ${e.camara.toFixed(1)} °C` : ""}</span>
          </li>
        ))}
      </ol>
    </>
  );
}

export function Dispositivo() {
  const { dispositivo: d, estado: e } = useFroster();
  const up = num(e.uptime_s) ? `${Math.floor(e.uptime_s / 86400)} d ${Math.floor((e.uptime_s % 86400) / 3600)} h ${Math.floor((e.uptime_s % 3600) / 60)} min` : "--";
  const barras = num(d.rssi) ? (d.rssi > -55 ? 4 : d.rssi > -67 ? 3 : d.rssi > -80 ? 2 : 1) : 0;
  return (
    <>
      <dl className="datos amplio">
        <Dato k="Firmware" v={d.firmware ?? "--"} />
        <Dato k="Encendido hace" v={up} />
        <Dato k="Último reinicio" v={d.ultimo_reinicio ?? "--"} clase={["panico", "watchdog", "voltaje bajo"].some((x) => d.ultimo_reinicio?.includes(x)) ? "mal" : ""} />
        <Dato k="IP" v={d.ip ?? "--"} />
        <Dato k="Red" v={`${d.ssid ?? "--"} · canal ${d.canal ?? "--"}`} />
        <Dato k="Señal" v={num(d.rssi) ? `${"▮".repeat(barras)}${"▯".repeat(4 - barras)} ${d.rssi} dBm` : "--"} clase={barras === 1 ? "ambar" : ""} />
        <Dato k="Caídas de Wi-Fi" v={d.caidas_wifi ?? "--"} />
        <Dato k="MAC" v={d.mac ?? "--"} />
        <Dato k="Temp. del chip" v={grados(d.temp_chip)} clase={d.temp_chip > 75 ? "mal" : ""} />
        <Dato k="CPU" v={d.cpu_mhz ? `${d.cpu_mhz} MHz` : "--"} />
        <Dato k="Memoria libre" v={d.heap_libre_kb ? `${d.heap_libre_kb} KB (mín. ${d.heap_min_kb} KB)` : "--"} />
        <Dato k="Programa" v={d.sketch_kb ? `${d.sketch_kb} KB · libre ${d.sketch_libre_kb} KB` : "--"} />
      </dl>
      <div className="botones">
        <BotonAccion nombre="reset_caidas" />
        <BotonAccion nombre="reiniciar" clase="peligro"
          confirmar="¿Reiniciar el ESP32? Las celdas se apagan y el control vuelve a arrancar (unos 15 s sin datos)." />
      </div>
    </>
  );
}

export function LatenciaVentana() {
  const { latencias, enviando, enviar, dispositivo } = useFroster();
  return <Latencia latencias={latencias} enviando={enviando} medir={() => enviar({}, "Prueba de latencia")} dispositivo={dispositivo} />;
}

const NO_INSTALADOS = [
  { clave: "rpm_ventiladores", nombre: "RPM de los ventiladores", unidad: "rpm",
    falta: "Conectar el cable amarillo (tacómetro) de cada ventilador a un GPIO libre con pull-up de 10 kΩ a 3.3 V. Ojo: el PWM de bajo lado corta la señal del tacómetro." },
  { clave: "voltaje_12v", nombre: "Voltaje de 12 V (celdas y bomba)", unidad: "V",
    falta: "Un INA226 en la línea de 12 V (bus I2C 21/22). Mide voltaje y corriente." },
  { clave: "corriente_celdas", nombre: "Corriente de las celdas", unidad: "A",
    falta: "El mismo INA226 con un shunt de 10 mΩ, o un INA219 por celda." },
  { clave: "voltaje_5v", nombre: "Voltaje de 5 V (ESP32)", unidad: "V",
    falta: "Divisor de voltaje 10k/10k a un ADC1 (GPIO35 o 36)." },
  { clave: "agua", nombre: "Temperatura del agua", unidad: "°C",
    falta: "DS18B20 en el waterblock (GPIO5) y SENSOR_AGUA 1 en el firmware." },
];

export function SinSensor() {
  const { lecturas: l } = useFroster();
  return (
    <>
      <p className="tenue pequeno">Estos datos no se pueden medir con el hardware actual. Cada espacio se activa solo cuando el
        firmware empiece a publicar la clave en <code>froster/lecturas</code>.</p>
      <ul className="no-instalados">
        {NO_INSTALADOS.map((s) => (
          <li key={s.clave} className={num(l[s.clave]) ? "est-ok" : "est-sin_sensor"}>
            <b>{s.nombre}</b>
            {num(l[s.clave])
              ? <span className="valor">{l[s.clave]} {s.unidad}</span>
              : <><span className="estado-txt">SIN SENSOR</span><div className="tenue pequeno">Falta: {s.falta}</div></>}
          </li>
        ))}
      </ul>
    </>
  );
}
