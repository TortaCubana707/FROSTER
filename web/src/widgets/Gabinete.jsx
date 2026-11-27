import { useState } from "react";
import { useFroster } from "../hooks/useFroster";

// Gabinete de FROSTER dibujado en SVG. Todo lo que se mueve sale de los datos reales del ESP32:
//  · aire: velocidad = PWM de los ventiladores; color del regreso = cámara contra el objetivo
//  · agua: solo circula con la bomba encendida; el regreso se pinta caliente si hay celdas
//  · calor expulsado: solo con alguna celda encendida
//  · cada pieza toma el color de su diagnóstico; clic en una pieza = qué revisar

const COLOR = { falla: "#f85149", aviso: "#d29922", activo: "#39c5cf", ok: "#3fb950", desconocido: "#6e7681", sin_sensor: "#484f58" };

const AIRE_FRIO = "M 400 150 C 330 150 250 170 150 175 C 110 178 95 150 100 120";
const AIRE_REGRESO = "M 100 120 C 110 70 200 62 300 66 C 360 68 395 90 400 110";
const AIRE_REGRESO_2 = "M 140 220 C 220 238 330 236 400 195";
const CALOR = "M 515 150 C 560 150 600 140 700 125";
const AGUA_IDA = "M 425 318 L 425 250 L 493 250 L 493 228";
const AGUA_VUELTA = "M 493 72 L 493 58 L 625 58 L 625 300 L 590 300";
const AGUA_CUBETA = "M 590 330 L 445 330";

function colorTemperatura(cam, sp) {
  if (typeof cam !== "number" || typeof sp !== "number") return "#8b949e";
  const d = cam - sp;
  if (d <= -0.5) return "#58a6ff";
  if (d <= 1) return "#39c5cf";
  if (d <= 2.5) return "#d29922";
  return "#f85149";
}

/** Partículas que recorren un camino: n puntos repartidos a lo largo de `dur` segundos. */
function Flujo({ camino, n, dur, color, r = 3.2 }) {
  if (!dur) return null;
  return (
    <g className="particulas" key={`${camino}-${dur}`}>
      {Array.from({ length: n }, (_, i) => (
        <circle key={i} r={r} fill={color} opacity="0.9">
          <animateMotion dur={`${dur}s`} repeatCount="indefinite" begin={`-${(dur * i) / n}s`} path={camino} />
        </circle>
      ))}
    </g>
  );
}

/** Pieza clicable: toma el color de su diagnóstico y parpadea si falla. */
function Pieza({ id, diag, sel, alElegir, children, etiqueta, ex = 0, ey = 0 }) {
  const d = diag[id];
  const est = d?.estado ?? "desconocido";
  return (
    <g className={`pieza est-${est} ${sel === id ? "elegida" : ""}`} style={{ "--c": COLOR[est] }}
      onClick={() => alElegir(id)} role="button" tabIndex={0}
      onKeyDown={(e) => (e.key === "Enter" || e.key === " ") && alElegir(id)}
      aria-label={`${d?.nombre ?? id}: ${est}${d?.valor ? `, ${d.valor}` : ""}`}>
      {children}
      {(est === "falla" || est === "aviso") && etiqueta && (
        <g className="globo" transform={`translate(${ex} ${ey})`}>
          <rect x="-4" y="-13" width={etiqueta.length * 6.6 + 18} height="18" rx="4" />
          <text x="5" y="0">⚠ {etiqueta}</text>
        </g>
      )}
    </g>
  );
}

function Ventilador({ x, y, girando, dur }) {
  return (
    <g transform={`translate(${x} ${y})`}>
      <circle r="17" className="marco-vent" />
      <g className={girando ? "girando" : ""} style={{ animationDuration: `${dur}s` }}>
        {[0, 120, 240].map((a) => (
          <path key={a} d="M0 0 C 4 -6 10 -12 2 -15 C -3 -10 -3 -5 0 0" transform={`rotate(${a})`} className="aspa" />
        ))}
      </g>
      <circle r="3" className="eje" />
    </g>
  );
}

function preferenciaAnimar() {
  try {
    const g = localStorage.getItem("froster.animar");
    if (g !== null) return g === "1";
    return !window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  } catch { return true; }
}

export default function Gabinete() {
  const { lecturas: l, estado: e, diagnostico, vivo } = useFroster();
  const [sel, setSel] = useState(null);
  const [animar, setAnimarEstado] = useState(preferenciaAnimar);
  const setAnimar = (v) => { setAnimarEstado(v); try { localStorage.setItem("froster.animar", v ? "1" : "0"); } catch { /* sin almacenamiento */ } };
  const diag = diagnostico.porId;

  const probandoBomba = e.prueba === "bomba";
  const pwm = probandoBomba ? 100 : e.bomba ? Number(e.pwm_fans) || 0 : 0;
  const mueve = vivo && pwm > 0;
  const durAire = mueve ? +(6 * 100 / Math.max(pwm, 15)).toFixed(1) : 0;
  const durVent = mueve ? +(0.5 * 100 / Math.max(pwm, 15)).toFixed(2) : 0;
  const durAgua = mueve && !e.fuga ? +(5 * 100 / Math.max(pwm, 15)).toFixed(1) : 0;
  const celdas = (e.celda_a ? 1 : 0) + (e.celda_b ? 1 : 0);
  const regreso = colorTemperatura(l.ambiente, e.setpoint);
  const sinHielo = !!e.sin_hielo;
  const elegido = sel && diag[sel];

  const resumen = `Gabinete: ${diagnostico.fallas.length} fallas, ventiladores al ${pwm} %, ${celdas} celdas encendidas`;

  return (
    <div className={`gabinete ${animar ? "animado" : ""}`}>
      <svg viewBox="0 0 720 420" role="img" aria-label={resumen}>
        <defs>
          <pattern id="aletas" width="6" height="6" patternUnits="userSpaceOnUse">
            <rect width="3" height="6" fill="#3987e5" opacity="0.55" />
          </pattern>
          <marker id="flecha" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse">
            <path d="M0 0 L10 5 L0 10 z" fill="#8b949e" />
          </marker>
        </defs>

        {/* Caja */}
        <rect x="60" y="30" width="600" height="365" rx="12" className="caja" />
        <text x="72" y="24" className="rotulo">GABINETE FROSTER</text>

        {/* Cámara */}
        <rect x="80" y="45" width="320" height="205" rx="8" className="camara" />
        <text x="92" y="240" className="rotulo">CÁMARA · {typeof l.ambiente === "number" ? `${l.ambiente.toFixed(1)} °C` : "--"} · objetivo {e.setpoint ?? "--"} °C</text>

        {/* Rutas fijas con flecha: la dirección se ve aunque la animación esté apagada */}
        {mueve && [AIRE_FRIO, AIRE_REGRESO, AIRE_REGRESO_2].map((d) => <path key={d} d={d} className="ruta" markerEnd="url(#flecha)" />)}
        {mueve && celdas > 0 && <path d={CALOR} className="ruta calor" markerEnd="url(#flecha)" />}

        {/* Aire: frío del bloque a la cámara, regreso al bloque, calor hacia afuera */}
        {celdas > 0 && <Flujo camino={CALOR} n={6} dur={durAire * 0.6} color="#f85149" />}
        <Flujo camino={AIRE_FRIO} n={9} dur={durAire} color="#58a6ff" />
        <Flujo camino={AIRE_REGRESO} n={8} dur={durAire} color={regreso} />
        <Flujo camino={AIRE_REGRESO_2} n={6} dur={durAire * 0.8} color={regreso} />
        {!mueve && <text x="160" y="150" className="quieto">{vivo ? "Ventilación apagada" : "Sin datos del ESP32"}</text>}

        {/* DHT22 */}
        <Pieza id="dht22" diag={diag} sel={sel} alElegir={setSel} etiqueta="DHT22" ex={120} ey={92}>
          <rect x="96" y="60" width="22" height="30" rx="3" className="cuerpo" />
          <text x="98" y="102" className="mini">DHT22</text>
        </Pieza>

        {/* Ventiladores de la cámara (impulsan el aire por las aletas del bloque) */}
        <Pieza id="ventiladores" diag={diag} sel={sel} alElegir={setSel} etiqueta="Ventiladores" ex={300} ey={268}>
          <Ventilador x={382} y={110} girando={mueve} dur={durVent} />
          <Ventilador x={382} y={195} girando={mueve} dur={durVent} />
          <Ventilador x={640} y={150} girando={mueve} dur={durVent} />
          <text x="624" y="186" className="mini">extrae</text>
        </Pieza>

        {/* Bloque frío con aletas + DS18B20 */}
        <Pieza id="ds18b20" diag={diag} sel={sel} alElegir={setSel} etiqueta="Bloque frío" ex={410} ey={262}>
          <rect x="405" y="70" width="44" height="160" rx="4" fill="url(#aletas)" className="bloque" />
          <circle cx="427" cy="150" r="5" className="cuerpo" />
          <text x="402" y="245" className="mini">{typeof l.placa_fria === "number" ? `${l.placa_fria.toFixed(1)} °C` : "bloque"}</text>
          {e.condensa && [0, 1, 2].map((i) => <circle key={i} cx={412 + i * 14} cy={234} r="2.5" className="gota" style={{ animationDelay: `${i * 0.4}s` }} />)}
        </Pieza>

        {/* Celdas Peltier */}
        <Pieza id="celda_a" diag={diag} sel={sel} alElegir={setSel} etiqueta="Celda A" ex={520} ey={95}>
          <rect x="451" y="82" width="16" height="62" rx="2" className={`celda ${e.celda_a ? "on" : ""}`} />
          <text x="453" y="118" className="mini v">A</text>
        </Pieza>
        <Pieza id="celda_b" diag={diag} sel={sel} alElegir={setSel} etiqueta="Celda B" ex={520} ey={215}>
          <rect x="451" y="156" width="16" height="62" rx="2" className={`celda ${e.celda_b ? "on" : ""}`} />
          <text x="453" y="192" className="mini v">B</text>
        </Pieza>

        {/* Waterblock (lado caliente) */}
        <Pieza id="agua" diag={diag} sel={sel} alElegir={setSel} etiqueta="Waterblock" ex={520} ey={155}>
          <rect x="469" y="72" width="48" height="156" rx="4" className="waterblock" />
          <text x="474" y="155" className="mini">agua</text>
        </Pieza>

        {/* Circuito de agua */}
        <path d={AGUA_IDA} className="manguera" />
        <path d={AGUA_VUELTA} className={`manguera ${celdas ? "caliente" : ""}`} />
        <Flujo camino={AGUA_IDA} n={5} dur={durAgua * 0.5} color="#58a6ff" r={2.6} />
        <Flujo camino={AGUA_VUELTA} n={8} dur={durAgua} color={celdas ? "#ff7b72" : "#58a6ff"} r={2.6} />
        <Flujo camino={AGUA_CUBETA} n={4} dur={durAgua * 0.5} color="#58a6ff" r={2.6} />

        {/* Cubeta de hielo */}
        <g className={`cubeta ${sinHielo ? "sin-hielo" : ""}`}>
          <rect x="445" y="290" width="160" height="85" rx="6" />
          {!sinHielo && [0, 1, 2, 3, 4].map((i) => <rect key={i} x={460 + i * 27} y={305 + (i % 2) * 18} width="18" height="16" rx="3" className="hielo" />)}
          <text x="455" y="368" className="mini">{sinHielo ? "SIN HIELO" : "cubeta de hielo"}</text>
        </g>

        {/* Bomba + sensor de fuga */}
        <Pieza id="bomba" diag={diag} sel={sel} alElegir={setSel} etiqueta="Bomba" ex={330} ey={312}>
          <circle cx="425" cy="330" r="17" className={`bomba ${mueve && !e.fuga ? "on" : ""}`} />
          <text x="413" y="334" className="mini">P</text>
        </Pieza>
        <Pieza id="fuga" diag={diag} sel={sel} alElegir={setSel} etiqueta={`Fuga ${l.fuga_pct ?? ""}%`} ex={330} ey={384}>
          <rect x="405" y="356" width="40" height="10" rx="2" className="cuerpo" />
          <text x="400" y="382" className="mini">sensor fuga</text>
          {e.fuga && [0, 1, 2, 3].map((i) => <circle key={i} cx={410 + i * 10} cy={372} r="3" className="gota fuga" style={{ animationDelay: `${i * 0.25}s` }} />)}
        </Pieza>

        {/* Electrónica */}
        <rect x="80" y="270" width="300" height="110" rx="8" className="bahia" />
        <text x="90" y="288" className="rotulo">ELECTRÓNICA</text>
        <Pieza id="esp32" diag={diag} sel={sel} alElegir={setSel} etiqueta="ESP32 sin latido" ex={94} ey={370}>
          <rect x="94" y="296" width="78" height="52" rx="4" className="placa" />
          <text x="112" y="326" className="mini">ESP32</text>
        </Pieza>
        <Pieza id="wifi" diag={diag} sel={sel} alElegir={setSel} etiqueta="Wi-Fi" ex={184} ey={262}>
          <path d="M160 296 v-10 M152 282 a12 12 0 0 1 16 0 M147 277 a19 19 0 0 1 26 0" className="antena" />
        </Pieza>
        <g className="mosfets">
          {["A", "B", "P"].map((m, i) => (
            <g key={m}>
              <rect x={190 + i * 32} y="300" width="24" height="34" rx="2"
                className={`mosfet ${(m === "A" && e.celda_a) || (m === "B" && e.celda_b) || (m === "P" && mueve) ? "on" : ""}`} />
              <text x={197 + i * 32} y="346" className="mini">{m}</text>
            </g>
          ))}
          <text x="188" y="362" className="mini">MOSFET D4184</text>
        </g>
        <Pieza id="buzzer" diag={diag} sel={sel} alElegir={setSel} etiqueta="Buzzer" ex={300} ey={262}>
          <circle cx="310" cy="312" r="10" className="cuerpo" />
          <text x="296" y="338" className="mini">buzzer</text>
        </Pieza>
        <circle cx="352" cy="312" r="7" className={`led nivel-${e.nivel ?? 0} ${e.alerta_id ? "alerta" : ""}`} />
        <text x="341" y="338" className="mini">LED</text>
      </svg>

      <div className="leyenda-gabinete">
        <span><i style={{ background: "#58a6ff" }} /> aire frío del bloque</span>
        <span><i style={{ background: regreso }} /> aire de regreso (cámara)</span>
        <span><i style={{ background: "#f85149" }} /> calor expulsado</span>
        <span><i className="borde" /> clic en una pieza para ver qué revisar</span>
        <button className="enlace pequeno" onClick={() => setAnimar(!animar)} aria-pressed={animar}>
          {animar ? "⏸ Pausar animación" : "▶ Animar flujo"}
        </button>
      </div>

      {elegido ? (
        <div className={`detalle-pieza est-${elegido.estado}`}>
          <b>{elegido.nombre}</b> · {elegido.valor}
          <div className="tenue">{elegido.detalle}</div>
          {elegido.queHacer && <div className="que-hacer">Qué hacer: {elegido.queHacer}</div>}
        </div>
      ) : diagnostico.fallas.length > 0 && (
        <div className="detalle-pieza est-falla">
          <b>{diagnostico.fallas.length === 1 ? "1 pieza con falla" : `${diagnostico.fallas.length} piezas con falla`}:</b>{" "}
          {diagnostico.fallas.map((f) => f.nombre).join(", ")}. Haz clic en la pieza roja.
        </div>
      )}
    </div>
  );
}
