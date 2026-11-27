// Interruptor accesible (role="switch") para las órdenes booleanas de config/.
export default function Interruptor({ texto, encendido, deshabilitado, ayuda, alCambiar }) {
  return (
    <label className={`interruptor ${deshabilitado ? "apagado" : ""}`} title={deshabilitado ? ayuda : undefined}>
      <span>{texto}</span>
      <button
        type="button"
        role="switch"
        aria-checked={!!encendido}
        disabled={deshabilitado}
        onClick={() => alCambiar(!encendido)}
      >
        <span className="perilla" />
      </button>
    </label>
  );
}
