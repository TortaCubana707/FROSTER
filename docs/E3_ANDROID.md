# E3 · App Android (Kevin)

> Consigna: app móvil nativa con conexión reactiva a Firebase, base local Room y un sistema de
> alertas, alarmas, mensajes emergentes (Toast/Snackbar) y notificaciones visuales/sonoras.
> Entregar: enlace a la rama de Android en GitHub y capturas de la pantalla principal, el historial
> local y ejemplos de alertas.

## Abrir el proyecto

1. Android Studio → **Open** → carpeta `android/` del repositorio (rama `e3-android`).
2. Esperar a que Gradle sincronice. No hace falta `google-services.json`: Firebase se configura en
   `FrosterApp.java` con los datos de `app/build.gradle`.
3. Run en un teléfono real conectado al Wi-Fi (el profe pide evidencia real, no emulador).
4. Iniciar sesión con la cuenta de Firebase del proyecto.

## Lo que ya está hecho (y compila)

| Requisito | Dónde |
|---|---|
| Interfaz en XML | `res/layout/activity_login.xml`, `activity_main.xml`, `activity_historial.xml` |
| Firebase reactivo | `datos/Repositorio.java`: `escuchar()` sobre `froster/lecturas` y `froster/estado` |
| Comandos con confirmación | `Repositorio.enviarComando()` escribe en `config/` con `cmd_id` y espera `ack/` |
| Room: entidades y DAO | `datos/Lectura.java`, `Evento.java`, `LecturaDao.java`, `EventoDao.java`, `BaseDatos.java` |
| Historial offline | `ui/HistorialActivity.java`: eventos y lecturas guardados en el teléfono |
| Picos de temperatura (Proyecto 1) | `EventoDao.picos()`: los eventos `tipo = "pico"` que manda el ESP32 |
| Alerta visual y sonora | `MainActivity.mostrarAlerta()`: banner rojo, `AlertDialog` y notificación con sonido |
| Snackbar / Toast | Confirmación de comandos (Snackbar con los ms), errores de inicio de sesión (Toast) |
| Sin ANR | Room corre en un hilo aparte; Firebase guarda caché en disco (`setPersistenceEnabled`) |
| ¿El ESP32 está vivo? | `MainActivity.revisarLatido`: "EN VIVO" / "SIN DATOS DEL ESP32" |

## Lo que falta (E3)

- [ ] **Maquetado**: tarjetas con `MaterialCardView`, colores por nivel, iconos. La pantalla actual
      es funcional pero sencilla a propósito.
- [ ] **Modo** (auto / manual / apagado) y celdas en manual: botones que llamen a
      `repo.enviarComando(...)` con `modo`, `manual_a`, `manual_b` (ver el contrato).
- [ ] Mostrar `pwm_fans`, `celda_a`, `celda_b`, `bomba` y `motivo` de `estado/`.
- [ ] Mostrar los eventos en vivo (`froster/eventos`) en la pantalla principal.
- [ ] Vibración o sonido distinto según `alerta_id` (opcional).
- [ ] Capturas para la entrega: pantalla principal, historial, una alerta (por ejemplo, mojar el
      sensor de fuga: `alerta_id = 4`) y un Snackbar de comando confirmado.

## Pruebas

`./gradlew testDebugUnitTest` corre `ContratoTest`. Si se agregan reglas de negocio (por ejemplo,
cuándo guardar una lectura), conviene probarlas igual.

## Errores comunes

- **"SIN DATOS DEL ESP32"**: la placa está apagada o tiene el firmware anterior (no escribe en `froster/`).
- **"El ESP32 no confirmó el comando"**: la placa no está conectada o no tiene el firmware de la E2.
- **Permission denied**: no se inició sesión, o las reglas de Firebase no permiten `froster/`.
