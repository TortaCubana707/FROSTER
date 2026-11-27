// Mensajes emergentes (tipo toast): se cierran solos o con la ×. Los graves se anuncian
// de inmediato a los lectores de pantalla (role="alert"); los demás, con calma (role="status").
export default function Avisos({ avisos, cerrar }) {
  return (
    <div className="avisos">
      {avisos.map((a) => (
        <div key={a.id} className={`aviso ${a.tipo}`} role={a.tipo === "grave" ? "alert" : "status"}>
          <span>{a.texto}</span>
          <button className="cerrar" onClick={() => cerrar(a.id)} aria-label="Cerrar aviso">×</button>
        </div>
      ))}
    </div>
  );
}
