// Mensajes emergentes (tipo toast) que desaparecen solos.
export default function Avisos({ avisos }) {
  return (
    <div className="avisos">
      {avisos.map((a) => <div key={a.id} className={`aviso ${a.tipo}`}>{a.texto}</div>)}
    </div>
  );
}
