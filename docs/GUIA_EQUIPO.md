# Guía del equipo FROSTER

Equipo: Emilio Garduño Servin, Kevin Alvarez De Jesus, Valeria Monserrat Benitez Alvarez y
Jesus Emmanuel Garcia Rodriguez. Repositorio: https://github.com/TortaCubana707/FROSTER

## Qué hay en el repositorio

| Carpeta | Qué es | Entregable |
|---|---|---|
| `firmware/FROSTER_Control/` | Firmware del ESP32 (Arduino IDE, C++) | E1 |
| `firebase/` | Reglas de la base y una muestra del árbol JSON | E2 |
| `docs/CONTRATO_FIREBASE.md` | **Rutas, tipos y comandos que comparten el ESP32, la app y la web** | E2 |
| `android/` | App Android FROSTER (Java + XML + Room + Firebase) | E3 |
| `web/` | Tablero en React (Vite + SDK modular de Firebase) | E4 |
| `dashboard/` | Tablero anterior en JavaScript simple; sirve de referencia y para medir latencia | E2 |
| `diagramas/`, `evidencias/`, `video_e1/` | Diagramas, fotos y video | E1 |

**Regla de oro:** antes de leer o escribir algo en Firebase, revisar
[`docs/CONTRATO_FIREBASE.md`](CONTRATO_FIREBASE.md). Si una ruta o un tipo no coincide con el
contrato, el ESP32 no lo va a entender.

## Ramas

| Rama | Para qué | Quién |
|---|---|---|
| `main` | Lo que ya funciona y se entregó | Se actualiza con Pull Requests |
| `e1-prototipo-firmware` | Entrega E1 (congelada) | — |
| `e2-firebase` | Entrega E2: firmware con stream, ack y telemetría filtrada | Emilio |
| `e3-android` | Entrega E3: app Android | Kevin |
| `e4-react` | Entrega E4: tablero React | Kevin |

Flujo de trabajo:

1. `git clone https://github.com/TortaCubana707/FROSTER.git` y `git checkout e3-android` (o `e4-react`).
2. Trabajar y hacer commits pequeños con el formato `tipo: descripción`
   (`feat: pantalla de historial`, `fix: el objetivo no se guardaba`).
3. `git push`. Cuando algo esté listo, abrir un Pull Request hacia `main` en GitHub.
4. No subir nunca `local.properties`, `.env.local`, `secretos.h` ni contraseñas: ya están en
   `.gitignore`.

Para poder hacer `push`, Emilio debe agregar a cada integrante en **Settings → Collaborators**.

## Cuenta de Firebase

Proyecto `sqliteemi`. La app y la web piden correo y contraseña en su pantalla de inicio; **la
contraseña nunca se escribe en el código**. Emilio la comparte por mensaje privado, no por el
repositorio.

## Antes de probar la app o la web

El ESP32 tiene que estar encendido con el firmware de la rama `e2-firebase` (escribe en `froster/`).
Si dice "SIN DATOS DEL ESP32", la placa está apagada o tiene el firmware anterior.

Más detalle de cada entregable: [E3_ANDROID.md](E3_ANDROID.md) y [E4_REACT.md](E4_REACT.md).
