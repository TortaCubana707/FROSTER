// Alarma sonora con Web Audio: no necesita archivos. Un patrón distinto por alerta, con la misma
// idea que el buzzer del ESP32 (agudos = congelamiento, largos = sin hielo, grave = sensor,
// alternados = fuga). Los navegadores solo dejan sonar después de un clic en la página.
let contexto = null;

// [frecuencia en Hz (0 = silencio), duración en s]
const PATRONES = {
  1: [[1320, 0.12], [0, 0.08], [1320, 0.12], [0, 0.08], [1320, 0.12]],
  2: [[880, 0.4], [0, 0.15], [880, 0.4]],
  3: [[220, 0.9]],
  4: [[880, 0.2], [660, 0.2], [880, 0.2], [660, 0.2]],
  resuelta: [[660, 0.12], [990, 0.18]],
};

export const audioListo = () => contexto?.state === "running";

/** Se llama desde un clic: crea o reanuda el audio. Devuelve true si ya puede sonar. */
export async function activarAudio() {
  try {
    contexto ??= new AudioContext();
    if (contexto.state !== "running") await contexto.resume();
  } catch {
    return false;
  }
  return audioListo();
}

function tocar(patron, volumen) {
  if (!audioListo() || !patron) return false;
  let t = contexto.currentTime + 0.02;
  for (const [hz, dur] of patron) {
    if (hz > 0) {
      const osc = contexto.createOscillator();
      const vol = contexto.createGain();
      osc.type = "square";
      osc.frequency.value = hz;
      vol.gain.setValueAtTime(volumen, t);
      vol.gain.exponentialRampToValueAtTime(0.001, t + dur);
      osc.connect(vol).connect(contexto.destination);
      osc.start(t);
      osc.stop(t + dur + 0.02);
    }
    t += dur;
  }
  return true;
}

export const sonarAlarma = (alertaId) => tocar(PATRONES[alertaId] ?? PATRONES[4], 0.12);
export const sonarResuelta = () => tocar(PATRONES.resuelta, 0.06);

// Notificación del sistema operativo (también avisa si la pestaña está en segundo plano).
export const notificacionesDisponibles = () => "Notification" in window;
export const permisoNotificaciones = () => (notificacionesDisponibles() ? Notification.permission : "denied");
export const pedirPermisoNotificaciones = () =>
  notificacionesDisponibles() ? Notification.requestPermission() : Promise.resolve("denied");

export function notificar(titulo, cuerpo) {
  if (permisoNotificaciones() !== "granted") return;
  try {
    new Notification(titulo, { body: cuerpo, tag: "froster-alerta", renotify: true });
  } catch {
    // En Android, Chrome solo permite notificaciones desde un service worker: queda el banner.
  }
}
