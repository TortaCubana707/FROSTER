export default function Tarjeta({ titulo, valor, detalle, children }) {
  return (
    <div className="tarjeta">
      <div className="etiqueta">{titulo}</div>
      {valor !== undefined && <div className="valor">{valor}</div>}
      {detalle && <div className="tenue">{detalle}</div>}
      {children}
    </div>
  );
}
