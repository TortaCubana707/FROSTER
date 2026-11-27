export default function Tarjeta({ titulo, valor, detalle, children, className = "", estilo }) {
  return (
    <section className={`tarjeta ${className}`} style={estilo}>
      <h2 className="etiqueta">{titulo}</h2>
      {valor !== undefined && <div className="valor">{valor}</div>}
      {detalle && <div className="tenue">{detalle}</div>}
      {children}
    </section>
  );
}
