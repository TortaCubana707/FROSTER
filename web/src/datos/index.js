// Punto único de acceso a los datos. Con ?demo en la URL se usa la simulación; si no, Firebase.
// Los componentes no saben de dónde vienen los datos: así se pueden revisar sin el prototipo.
import { fuenteFirebase } from "./fuenteFirebase";
import { fuenteDemo } from "./fuenteDemo";

export const fuente = new URLSearchParams(window.location.search).has("demo") ? fuenteDemo : fuenteFirebase;
