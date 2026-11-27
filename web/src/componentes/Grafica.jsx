import { useEffect, useMemo, useRef, useState } from "react";

// Gráfica de los últimos 30 min en SVG propio: sin dependencias (menos peso y nada que auditar).
// Dos series con colores validados para daltonismo sobre la tarjeta oscura (CVD ΔE 26.8)
// y el objetivo como línea de referencia gris punteada.
const SERIES = [
  { clave: "camara", nombre: "Cámara", color: "var(--serie-camara)" },
  { clave: "bloque", nombre: "Bloque frío", corto: "Bloque", color: "var(--serie-bloque)" },
];
const ALTO = 260;
const MARGEN = { arriba: 12, derecha: 108, abajo: 26, izquierda: 40 };

const hora = (t) => new Date(t).toLocaleTimeString("es-MX", { hour: "2-digit", minute: "2-digit" });
const horaLarga = (t) => new Date(t).toLocaleTimeString("es-MX");
const grados = (v) => (typeof v === "number" ? `${v.toFixed(1)} °C` : "--");

// Marcas del eje en pasos "redondos" (0.5, 1, 2, 2.5, 5 o 10 °C)
function marcas(min, max, cuantas = 5) {
  const crudo = (max - min) / cuantas;
  const paso = [0.5, 1, 2, 2.5, 5, 10].find((p) => p >= crudo) ?? 10;
  const lista = [];
  for (let v = Math.ceil(min / paso) * paso; v <= max + 1e-9; v += paso) lista.push(Math.round(v * 10) / 10);
  return lista;
}

export default function Grafica({ puntos }) {
  const caja = useRef(null);
  const [ancho, setAncho] = useState(640);
  const [foco, setFoco] = useState(null);       // índice del punto bajo el cursor
  const [verTabla, setVerTabla] = useState(false);

  useEffect(() => {
    if (!caja.current) return undefined;
    const ro = new ResizeObserver(([e]) => setAncho(Math.max(280, e.contentRect.width)));
    ro.observe(caja.current);
    return () => ro.disconnect();
  }, []);

  const escala = useMemo(() => {
    if (puntos.length < 2) return null;
    const valores = puntos.flatMap((p) => [p.camara, p.bloque, p.objetivo]).filter((v) => typeof v === "number");
    const min = Math.floor(Math.min(...valores) - 1);
    const max = Math.ceil(Math.max(...valores) + 1);
    const t0 = puntos[0].t, t1 = puntos[puntos.length - 1].t;
    const w = ancho - MARGEN.izquierda - MARGEN.derecha;
    const h = ALTO - MARGEN.arriba - MARGEN.abajo;
    return {
      min, max, t0, t1,
      x: (t) => MARGEN.izquierda + ((t - t0) / Math.max(1, t1 - t0)) * w,
      y: (v) => MARGEN.arriba + (1 - (v - min) / (max - min)) * h,
    };
  }, [puntos, ancho]);

  const linea = (clave) => {
    let d = "";
    let abierta = false;
    for (const p of puntos) {
      if (typeof p[clave] !== "number") { abierta = false; continue; }   // hueco si faltó el dato
      d += `${abierta ? "L" : "M"}${escala.x(p.t).toFixed(1)},${escala.y(p[clave]).toFixed(1)}`;
      abierta = true;
    }
    return d;
  };

  const alMover = (e) => {
    if (!escala) return;
    const x = e.clientX - e.currentTarget.getBoundingClientRect().left;
    let mejor = 0;
    puntos.forEach((p, i) => {
      if (Math.abs(escala.x(p.t) - x) < Math.abs(escala.x(puntos[mejor].t) - x)) mejor = i;
    });
    setFoco(mejor);
  };

  const ultimo = puntos[puntos.length - 1];
  const p = foco !== null ? puntos[foco] : null;

  // Etiquetas directas al final de cada línea, separadas si las dos temperaturas quedan juntas
  const etiquetas = escala
    ? SERIES.filter((s) => typeof ultimo[s.clave] === "number")
        .map((s) => ({ ...s, valor: ultimo[s.clave], yPunto: escala.y(ultimo[s.clave]), y: escala.y(ultimo[s.clave]) }))
        .sort((a, b) => a.y - b.y)
    : [];
  for (let i = 1; i < etiquetas.length; i++) {
    if (etiquetas[i].y - etiquetas[i - 1].y < 14) etiquetas[i].y = etiquetas[i - 1].y + 14;
  }

  return (
    <section className="tarjeta grafica">
      <div className="titulo-grafica">
        <h2 className="etiqueta">Temperatura · últimos 30 min</h2>
        <div className="leyenda">
          {SERIES.map((s) => (
            <span key={s.clave}><i style={{ background: s.color }} />{s.nombre}</span>
          ))}
          <span><i className="punteada" />Objetivo</span>
          <button className="enlace" onClick={() => setVerTabla((v) => !v)}>{verTabla ? "Ver gráfica" : "Ver tabla"}</button>
        </div>
      </div>

      <div ref={caja} className="lienzo">
        {!escala && <p className="tenue vacio">Esperando lecturas del ESP32… la gráfica suma un punto con cada dato nuevo.</p>}

        {escala && !verTabla && (
          <svg width={ancho} height={ALTO} role="img"
            aria-label={`Cámara ${grados(ultimo.camara)}, bloque frío ${grados(ultimo.bloque)}, objetivo ${grados(ultimo.objetivo)}`}
            onMouseMove={alMover} onMouseLeave={() => setFoco(null)}>
            {marcas(escala.min, escala.max).map((v) => (
              <g key={v}>
                <line className="rejilla-graf" x1={MARGEN.izquierda} x2={ancho - MARGEN.derecha} y1={escala.y(v)} y2={escala.y(v)} />
                <text className="eje" x={MARGEN.izquierda - 6} y={escala.y(v) + 4} textAnchor="end">{v}</text>
              </g>
            ))}
            {/* En pantallas angostas solo inicio y fin, para que las horas no se encimen */}
            {(ancho < 480 ? [escala.t0, escala.t1] : [escala.t0, (escala.t0 + escala.t1) / 2, escala.t1]).map((t, i, lista) => (
              <text key={i} className="eje" x={escala.x(t)} y={ALTO - 6}
                textAnchor={i === 0 ? "start" : i === lista.length - 1 ? "end" : "middle"}>{hora(t)}</text>
            ))}

            <path d={linea("objetivo")} className="linea-objetivo" />
            {SERIES.map((s) => <path key={s.clave} d={linea(s.clave)} stroke={s.color} className="linea" />)}

            {/* Etiquetas directas al final de cada línea */}
            {etiquetas.map((s) => (
              <g key={s.clave}>
                <circle cx={escala.x(ultimo.t)} cy={s.yPunto} r="4" fill={s.color} className="punto" />
                <text className="etiqueta-linea" x={escala.x(ultimo.t) + 8} y={s.y + 4}>
                  {s.corto ?? s.nombre} {s.valor.toFixed(1)} °C
                </text>
              </g>
            ))}

            {p && (
              <g>
                <line className="mira" x1={escala.x(p.t)} x2={escala.x(p.t)} y1={MARGEN.arriba} y2={ALTO - MARGEN.abajo} />
                {SERIES.filter((s) => typeof p[s.clave] === "number").map((s) => (
                  <circle key={s.clave} cx={escala.x(p.t)} cy={escala.y(p[s.clave])} r="4" fill={s.color} className="punto" />
                ))}
              </g>
            )}
          </svg>
        )}

        {escala && !verTabla && p && (
          <div className="tooltip" style={{ left: Math.min(escala.x(p.t) + 12, ancho - 170), top: 8 }}>
            <strong>{horaLarga(p.t)}</strong>
            {SERIES.map((s) => (
              <div key={s.clave}><i style={{ background: s.color }} />{s.nombre} <b>{grados(p[s.clave])}</b></div>
            ))}
            <div><i className="punteada" />Objetivo <b>{grados(p.objetivo)}</b></div>
          </div>
        )}

        {escala && verTabla && (
          <table className="tabla">
            <thead><tr><th>Hora</th><th>Cámara</th><th>Bloque frío</th><th>Objetivo</th></tr></thead>
            <tbody>
              {puntos.slice(-12).reverse().map((q) => (
                <tr key={q.t}><td>{horaLarga(q.t)}</td><td>{grados(q.camara)}</td><td>{grados(q.bloque)}</td><td>{grados(q.objetivo)}</td></tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </section>
  );
}
