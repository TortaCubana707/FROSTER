// Alarma sonora con Web Audio: no necesita archivos de audio.
let contexto = null;

export function sonarAlarma() {
  contexto ??= new AudioContext();
  const ahora = contexto.currentTime;
  [880, 660, 880].forEach((hz, i) => {
    const osc = contexto.createOscillator();
    const vol = contexto.createGain();
    osc.frequency.value = hz;
    vol.gain.setValueAtTime(0.2, ahora + i * 0.25);
    vol.gain.exponentialRampToValueAtTime(0.001, ahora + i * 0.25 + 0.22);
    osc.connect(vol).connect(contexto.destination);
    osc.start(ahora + i * 0.25);
    osc.stop(ahora + i * 0.25 + 0.23);
  });
}
