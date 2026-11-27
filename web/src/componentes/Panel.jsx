import { useCallback, useEffect, useRef, useState } from "react";
import { RUTAS } from "../contrato";
import { fuente } from "../datos";
import { useNodo } from "../hooks/useNodo";
import { useAvisos } from "../hooks/useAvisos";
import { useAlarma } from "../hooks/useAlarma";
import { FrosterContexto, useDatosFroster } from "../hooks/useFroster";
import { ACOMODO_FABRICA, CATALOGO, sanear } from "../widgets/catalogo";
import Encabezado from "./Encabezado";
import BannerAlerta from "./BannerAlerta";
import Avisos from "./Avisos";

/**
 * Panel de administración estilo pfSense: ventanas que se arrastran por la barra de título,
 * se agregan con "+", se quitan con ×, se minimizan y cambian de ancho. El acomodo se guarda en
 * froster/ui/layout; la app Android lo lee y muestra las mismas ventanas en el mismo orden.
 */
export default function Panel() {
  const { avisos, avisar, cerrar } = useAvisos();
  const datos = useDatosFroster(avisar);
  const { silenciada, silenciar } = useAlarma(datos.estado.alerta_id ?? 0, avisar);

  const ui = useNodo(RUTAS.ui);
  const [acomodo, setAcomodo] = useState(null);
  const [arrastrando, setArrastrando] = useState(null);
  const [sobre, setSobre] = useState(null);
  const [menu, setMenu] = useState(false);
  const pendiente = useRef(null);

  // El acomodo guardado manda; si no hay, el de fábrica. Una vez cargado, lo local manda.
  useEffect(() => {
    if (ui === null || acomodo) return;
    setAcomodo(sanear(ui.layout) ?? ACOMODO_FABRICA);
  }, [ui, acomodo]);

  const guardar = useCallback((nuevo) => {
    setAcomodo(nuevo);
    clearTimeout(pendiente.current);
    pendiente.current = setTimeout(() => {
      fuente.actualizar(RUTAS.ui, { layout: nuevo, actualizado: Date.now() })
        .catch((e) => avisar(`No se guardó el acomodo: ${e.message}`, "grave"));
    }, 600);
  }, [avisar]);

  if (!acomodo) return <p className="cargando">Cargando el panel…</p>;

  const visibles = new Set(acomodo.map((v) => v.id));
  const ocultas = Object.keys(CATALOGO).filter((id) => !visibles.has(id));
  const cambiar = (id, cambio) => guardar(acomodo.map((v) => (v.id === id ? { ...v, ...cambio } : v)));
  const quitar = (id) => guardar(acomodo.filter((v) => v.id !== id));
  const agregar = (id) => { guardar([{ id, w: CATALOGO[id].ancho, min: false }, ...acomodo]); setMenu(false); };
  const mover = (id, destino, despues) => {
    if (id === destino) return;
    const sin = acomodo.filter((v) => v.id !== id);
    const i = sin.findIndex((v) => v.id === destino);
    sin.splice(despues ? i + 1 : i, 0, acomodo.find((v) => v.id === id));
    guardar(sin);
  };
  const correr = (id, paso) => {
    const i = acomodo.findIndex((v) => v.id === id);
    const j = i + paso;
    if (j < 0 || j >= acomodo.length) return;
    const n = [...acomodo];
    [n[i], n[j]] = [n[j], n[i]];
    guardar(n);
  };

  return (
    <FrosterContexto.Provider value={{ ...datos, avisar }}>
      <main className="panel">
        <Encabezado vivo={datos.vivo} conectado={datos.conectado} demo={datos.demo}
          alSalir={() => fuente.salir()} alSimularFuga={() => fuente.simularFuga?.()} />
        <BannerAlerta alertaId={datos.estado.alerta_id ?? 0} textoEsp32={datos.estado.alerta}
          silenciada={silenciada} alSilenciar={silenciar} />

        <div className="barra-panel">
          <span className="migas">Estado / <b>Panel</b></span>
          <span className="tenue pequeno">Arrastra una ventana por su título para moverla. La app ve el mismo acomodo.</span>
          <div className="acciones">
            <div className="menu-agregar">
              <button onClick={() => setMenu(!menu)} aria-expanded={menu} disabled={!ocultas.length}
                title={ocultas.length ? "Agregar ventana" : "Todas las ventanas están en el panel"}>＋ Agregar ventana</button>
              {menu && (
                <ul role="menu">
                  {ocultas.map((id) => <li key={id}><button role="menuitem" onClick={() => agregar(id)}>{CATALOGO[id].titulo}</button></li>)}
                </ul>
              )}
            </div>
            <button onClick={() => window.confirm("¿Volver al acomodo de fábrica?") && guardar(ACOMODO_FABRICA)}>Restaurar acomodo</button>
          </div>
        </div>

        <div className="rejilla-panel">
          {acomodo.map((v) => {
            const { titulo, comp: Contenido } = CATALOGO[v.id];
            return (
              <section key={v.id}
                className={`ventana w${v.w} ${v.min ? "minimizada" : ""} ${arrastrando === v.id ? "arrastrando" : ""} ${sobre?.id === v.id ? (sobre.despues ? "soltar-despues" : "soltar-antes") : ""}`}
                onDragOver={(e) => {
                  if (!arrastrando) return;
                  e.preventDefault();
                  const r = e.currentTarget.getBoundingClientRect();
                  setSobre({ id: v.id, despues: e.clientX > r.left + r.width / 2 });
                }}
                onDragLeave={() => setSobre(null)}
                onDrop={(e) => { e.preventDefault(); if (arrastrando) mover(arrastrando, v.id, sobre?.despues); setSobre(null); setArrastrando(null); }}>
                <header className="titulo-ventana" draggable
                  onDragStart={(e) => { e.dataTransfer.setData("text/plain", v.id); e.dataTransfer.effectAllowed = "move"; setArrastrando(v.id); }}
                  onDragEnd={() => { setArrastrando(null); setSobre(null); }}>
                  <span className="asa" aria-hidden="true">⠿</span>
                  <h2>{titulo}</h2>
                  <div className="botones-ventana">
                    <button onClick={() => correr(v.id, -1)} aria-label={`Mover ${titulo} antes`} title="Mover antes">◀</button>
                    <button onClick={() => correr(v.id, 1)} aria-label={`Mover ${titulo} después`} title="Mover después">▶</button>
                    <button onClick={() => cambiar(v.id, { w: (v.w % 4) + 1 })} aria-label={`Ancho de ${titulo}: ${v.w} de 4`} title={`Ancho ${v.w}/4`}>⇔</button>
                    <button onClick={() => cambiar(v.id, { min: !v.min })} aria-label={v.min ? `Abrir ${titulo}` : `Minimizar ${titulo}`} title={v.min ? "Abrir" : "Minimizar"}>{v.min ? "＋" : "−"}</button>
                    <button onClick={() => quitar(v.id)} aria-label={`Quitar ${titulo}`} title="Quitar del panel">×</button>
                  </div>
                </header>
                {!v.min && <div className="cuerpo-ventana"><Contenido /></div>}
              </section>
            );
          })}
        </div>

        <footer className="tenue">
          ESP32 → Wi-Fi → Firebase (<code>froster/</code>) → este panel (administrador) y la app Android (vista de usuario).
        </footer>
        <Avisos avisos={avisos} cerrar={cerrar} />
      </main>
    </FrosterContexto.Provider>
  );
}
