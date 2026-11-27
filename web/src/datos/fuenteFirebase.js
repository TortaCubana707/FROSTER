// Fuente de datos real: Firebase Realtime Database + Authentication (SDK modular).
import { ref, onValue, update, query, orderByKey, limitToLast } from "firebase/database";
import { onAuthStateChanged, signInWithEmailAndPassword, signOut } from "firebase/auth";
import { auth, db } from "../firebase";

export const fuenteFirebase = {
  demo: false,

  /** Escucha una ruta. Con { ultimos: n } devuelve un arreglo con los n hijos más recientes. */
  escuchar(ruta, alCambiar, { ultimos } = {}) {
    const r = ref(db, ruta);
    if (!ultimos) return onValue(r, (s) => alCambiar(s.val()));
    return onValue(query(r, orderByKey(), limitToLast(ultimos)), (s) => {
      const lista = [];
      s.forEach((hijo) => {
        lista.push({ id: hijo.key, ...hijo.val() });
      });
      alCambiar(lista);
    });
  },

  actualizar: (ruta, cambios) => update(ref(db, ruta), cambios),
  alCambiarSesion: (alCambiar) => onAuthStateChanged(auth, alCambiar),
  entrar: (correo, clave) => signInWithEmailAndPassword(auth, correo, clave),
  salir: () => signOut(auth),
  /** Rol de la cuenta (roles/{uid}): "admin" o null. Se asigna a mano en la consola de Firebase. */
  rol: (uid, alCambiar) => onValue(ref(db, `roles/${uid}`), (s) => alCambiar(s.val()), () => alCambiar(null)),
  historialInicial: () => null,
};
