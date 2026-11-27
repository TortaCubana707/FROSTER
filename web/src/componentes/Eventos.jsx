import { TIPOS_EVENTO } from "../contrato";

const hora = (t) => (t ? new Date(t).toLocaleTimeString("es-MX") : "--");

// Bitácora en vivo de froster/eventos: cambios de nivel, alertas y picos de temperatura.
export default function Eventos({ eventos }) {
  return (
    <section className="tarjeta eventos">
      <h2 className="etiqueta">Últimos eventos</h2>
      {eventos === null && <p className="tenue">Cargando…</p>}
      {eventos?.length === 0 && <p className="tenue">Todavía no hay eventos.</p>}
      <ol>
        {eventos?.map((e) => (
          <li key={e.id} className={`evento ${e.tipo}`}>
            <span className="texto">{e.texto}</span>
            <span className="tenue">
              {TIPOS_EVENTO[e.tipo] ?? "Evento"} · {hora(e.ts)}
              {typeof e.camara === "number" ? ` · cámara ${e.camara.toFixed(1)} °C` : ""}
            </span>
          </li>
        ))}
      </ol>
    </section>
  );
}
