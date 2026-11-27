import { ALERTAS } from "../contrato";

// Banner que parpadea mientras estado/alerta_id > 0. Texto + icono + color: nunca solo color.
export default function BannerAlerta({ alertaId, textoEsp32, silenciada, alSilenciar }) {
  if (!alertaId) return null;
  const { nombre, queHacer } = ALERTAS[alertaId] ?? { nombre: textoEsp32 || "Alerta", queHacer: "" };
  return (
    <div className="banner-alerta" role="alert">
      <span className="icono" aria-hidden="true">⚠</span>
      <div>
        <strong>{nombre}</strong>
        {queHacer && <p>{queHacer}</p>}
      </div>
      <button onClick={alSilenciar} disabled={silenciada}>
        {silenciada ? "Alarma silenciada" : "Silenciar alarma"}
      </button>
    </div>
  );
}
