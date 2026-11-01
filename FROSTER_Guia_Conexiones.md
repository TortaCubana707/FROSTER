# FROSTER — Guía de conexiones paso a paso

Versión del 24 oct 2026. Fuente de 12 V 5 A, jumper de la base en 5V, LCD + OLED, LED RGB y voz de FROSTER en la app,
sin DFPlayer. Pensada para armarse sin experiencia previa.

---

## Antes de empezar

### Herramientas

| Herramienta | Para qué |
|---|---|
| Multímetro | Medir voltajes y ajustar los reductores |
| Desarmador plano chico | Tornillos de las borneras y potenciómetros |
| Pinzas de corte | Pelar y cortar cable |
| Cables Dupont **hembra-hembra** | Sensores, pantalla, LED y buzzer |
| Cable **calibre 18**, rojo y negro | Todo lo que lleva 12 V o va a las celdas |
| Resistencias: 3 de **220 Ω** | LED RGB |
| Diodo **1N4007** | Protección de la bomba |

### Cinco reglas

1. **Fuente desenchufada** mientras conectas. Al desenchufar, espera 30 segundos.
2. **Rojo = positivo (+), negro = negativo (−).** Nunca los cruces.
3. En los tornillos, mete **solo el cobre pelado** (unos 5 mm) y aprieta hasta que no salga al jalarlo.
4. Revisa **dos veces** cada conexión antes de dar corriente.
5. Si algo huele a quemado o se calienta mucho: **desenchufa de inmediato**.

### Cómo leer la placa de expansión

Cada pin tiene una fila de tres patitas:

| Letra | Significa | Con el jumper en 5V da |
|---|---|---|
| **G** | Tierra (−) | 0 V |
| **V** | Voltaje (+) | 5 V |
| **S** | Señal (datos o control) | La señal del ESP32 |

**"S del D25"** = la patita S de la fila marcada **D25**.

---

## Parte 1 — ESP32 en la placa

| Paso | Qué hacer | Cómo saber que quedó bien |
|---|---|---|
| 1 | Inserta el ESP32 en la placa, con su USB **del mismo lado** que los USB de la placa | Ninguna patita quedó fuera |
| 2 | Jumper amarillo en las patitas **V y 5V** | — |
| 3 | Conecta la power bank al **USB-C** de la placa | Se enciende el LED **PWR** |
| 4 | Multímetro en **V⎓ 20V**: punta negra en una **G** y roja en una **V** | Marca ≈ **5 V** |
| 5 | Desconecta la power bank | — |

**No uses el jack DC de la placa.** Pasa por un regulador lineal que a 12 V se sobrecalienta.

---

## Parte 2 — Sensores

Cada uno entra directo en su fila.

### Termómetro — Dentro del bloque frío (para el congelamiento)

| Pin del adaptador | Placa |
|---|---|
| GND | **G** del D18 |
| VCC | **V** del D18 |
| DAT | **S** del D18 |

La fila **D5 queda libre**: ya no hay termómetro en el agua. Si se acaba el hielo o se para la
bomba, el control lo nota porque el bloque deja de enfriar (con una celda encendida 3 minutos,
el bloque debe estar al menos 2 °C más frío que la cámara) y apaga las celdas.

**Revisa el orden de los pines del adaptador antes de conectar:** un DS18B20 al revés se quema.

### DHT22 — temperatura interna de la caja (tapado de la luz directa del foco)

| Pin | Placa |
|---|---|
| GND (−) | **G** del D4 |
| VCC (+) | **V** del D4 |
| DATA (OUT) | **S** del D4 |

---

## Parte 3 — Pantalla LCD

| Pin de la pantalla | Placa |
|---|---|
| GND | **G** del D21 |
| VCC | **V** del D21 |
| SDA | **S** del D21 |
| SCL | **S** del D22 |

✔ Con la power bank conectada se enciende la luz de la pantalla. Sin programa no muestra
letras. Si después no se ven, gira el potenciómetro azul de atrás.

### Pantalla OLED 0.96" (en su propio bus, separada de la LCD)

| Pin de la OLED | Placa |
|---|---|
| GND | **G** del D17 |
| VCC | **V** del D17 |
| SCL | **S** del D17 |
| SDA | **S** del D16 |

Cada pantalla tiene sus propios pines: la LCD usa D21/D22 y la OLED D16/D17. En la OLED el orden
es GND·VCC·SCL·SDA y en la LCD GND·VCC·SDA·SCL: **guíate por las letras**. Si la OLED no está
conectada, el programa sigue igual con la LCD.

### Qué muestra cada una

| Pantalla | Función | Contenido |
|---|---|---|
| **OLED** | Lo que decide el control | Nivel y cámara en grande, celdas y bomba encendidas, tendencia, objetivo, gráfica de los últimos ~4 min y el motivo de la decisión |
| **LCD 16×2** | El módulo, el uso y la red (cambia cada 3 s) | 1) bloque frío, agua, punto de rocío y humedad · 2) horas de uso de cada celda · 3) WiFi, Firebase e IP · y las alertas intercaladas |

---

## Parte 4 — LED RGB

### Averigua qué tipo es

La pata **más larga** es la común. Con la power bank conectada:

| Prueba | Si enciende |
|---|---|
| Pata larga a **G**; otra pata, con una resistencia de 220 Ω, a una **V** | Es de **cátodo común** |
| Pata larga a una **V**; otra pata, con una resistencia de 220 Ω, a **G** | Es de **ánodo común** |

### Conexión

| Pata del LED | Placa |
|---|---|
| R (rojo) | Resistencia de **220 Ω** → **S** del D32 |
| G (verde) | Resistencia de **220 Ω** → **S** del D33 |
| B (azul) | Resistencia de **220 Ω** → **S** del D13 |
| Común — si es de **cátodo común** | **G** del D32 |
| Común — si es de **ánodo común** | Patita libre **3.3V** del jumper (a 5 V el rojo no se apagaría del todo) |

Si es de ánodo común, en el código se cambia `LED_ANODO_COMUN` a `1`.

---

## Parte 5 — Buzzer pasivo de 3 pines

| Pin del buzzer | Placa |
|---|---|
| S (señal) | **S** del D23 |
| VCC o + | **V** del D23 |
| GND o − | **G** del D23 |

Al encender la placa debe sonar una melodía de 3 notas. Si zumba o se calienta en silencio, en el
código se cambia `BUZZER_ACTIVO_BAJO` a `1`.

Además, en la app, el botón **"Hablar con FROSTER"** abre un asistente que contesta en voz alta y
avisa las alertas.

Aquí termina todo lo que se programa.

---

## Parte 6 — Fuente 12 V 5 A

### Entrada

| Cable de la clavija | Bornera |
|---|---|
| Negro (pata delgada) | L |
| Blanco (pata ancha) | N |
| Verde (pata redonda) | ⏚ |

### Primera prueba

| Paso | Qué hacer | Cómo saber que quedó bien |
|---|---|---|
| 1 | Con la clavija desenchufada, continuidad entre cada color y su pata | Pita |
| 2 | Cierra la tapa transparente de L, N y ⏚ | No se ve cobre |
| 3 | Continuidad entre V+ y V− de la salida | **No pita** |
| 4 | Multímetro en **V⎓ 20V**. Enchufa sin nada en la salida y mide V+ a V− | Entre **11.8 y 12.3 V** |
| 5 | Desenchufa y espera 30 segundos | — |

Ponla en la caja de control, lejos de la bandeja y de la cubeta: su rejilla deja pasar agua.

---

## Parte 6b — Foco amarillo (carga de calor de la cámara)

Va **aparte**, a la pared, con su propio apagador. **No se conecta al ESP32 ni a la fuente de
12 V.** Hace el papel de la computadora que el módulo tiene que enfriar.

| Cable de la clavija | Va a |
|---|---|
| Negro (fase) | Apagador → contacto del **centro** del socket |
| Blanco (neutro) | **Rosca** del socket |

| Regla | Por qué |
|---|---|
| Foco **incandescente o halógeno** de **25 a 40 W** | Un LED casi no calienta. Con 25 W el módulo le gana; con 40 W llega a su límite; con 60 W o más la cámara siempre sube |
| El apagador corta la **fase**, nunca el neutro | Así, con el apagador abierto, el socket queda sin voltaje peligroso |
| Socket de **porcelana** | Uno de plástico se derrite con el calor del foco |
| Cables por **arriba** de la cámara | Lejos del agua, la cubeta y la bandeja |
| Empalmes con **termorretráctil** | Nunca solo con cinta |
| Tapa el **DHT22** de la luz directa del foco | Si no, mide la luz del foco en lugar del aire |

---

## Parte 7 — Reductores XL4016 (dos): ajústalos primero

Para ajustarlos se conectan **directo a la fuente**. Al final de la Parte 8 su IN− pasa al
D4184; mientras eso no pase, el reductor no enciende.

### Conexión para ajustar

| Fuente | Va a | Cable |
|---|---|---|
| V+ #1 | IN+ del XL4016 #1 **y** IN+ del XL4016 #2 | Rojo, calibre 18 |
| V− #1 | IN− del XL4016 #1 **y** IN− del XL4016 #2 (**provisional**) | Negro, calibre 18 |

### Ajuste, uno por uno y **con la salida vacía**

| Paso | Qué hacer | Cómo saber que quedó bien |
|---|---|---|
| 1 | Enchufa la fuente | Se enciende el LED del XL4016 |
| 2 | Multímetro en **V⎓ 20V**, puntas en OUT+ / OUT− | Marca algún voltaje |
| 3 | Gira uno de los dos potenciómetros. **El que mueve la lectura es el de voltaje (CV)** | — |
| 4 | Gira el de voltaje hasta **5.0 V**. Son de muchas vueltas: pueden ser 10 o 20 | Marca **5.0 V** |
| 5 | Pasa la punta roja al agujero **10A** y la perilla a **10A⎓** | — |
| 6 | Puntas en OUT+ / OUT− y gira el **otro** potenciómetro (CC) hasta **3.0 A**, en menos de 10 segundos | Marca **3.0 A** |
| 7 | **Regresa la punta roja a VΩ.** Si la dejas en 10A, la próxima medición de voltaje será un corto | — |
| 8 | Desenchufa y repite con el otro XL4016 | — |
| 9 | Desenchufa y **quita los dos cables provisionales de IN−** | IN− de los dos XL4016 quedan libres |

---

## Parte 8 — Interruptores D4184 (tres)

Son como el de la foto: una **bornera azul de 3** y una **bornera verde de 2**, en extremos
opuestos de la placa.

| Bornera | Tornillo | Significa |
|---|---|---|
| Azul | **+** | +12 V (alimenta al módulo) |
| Azul | **LOAD** | Negativo de lo que se va a encender |
| Azul | **−** | Tierra de los 12 V |
| Verde | **PWM** | Señal del ESP32 |
| Verde | **GND** | Tierra del ESP32 |

La bornera verde está **aislada** con un optoacoplador: el lado del ESP32 y el de 12 V no se
tocan. **No hay que unir las tierras.**

### Conexión

| D4184 | + (azul) | LOAD (azul) | − (azul) | PWM (verde) | GND (verde) |
|---|---|---|---|---|---|
| **#1** (celda A) | IN+ del XL4016 #1 (cable corto) | IN− del XL4016 #1 | V− #1 de la fuente | **S** del D25 | **G** del D25 |
| **#2** (celda B) | IN+ del XL4016 #2 (cable corto) | IN− del XL4016 #2 | V− #1 de la fuente | **S** del D26 | **G** del D26 |
| **#3** (bomba y ventiladores) | V+ #2 de la fuente | Negros de la bomba y los ventiladores | V− #2 de la fuente | **S** del D27 | **G** del D27 |

### Por qué los D4184 de las celdas van **antes** del XL4016

- Este módulo necesita **6 V o más** en su `+` para encender bien su MOSFET. Después del
  XL4016 solo hay unos 4 V: se quedaría a medias y se calentaría.
- El XL4016 mide la corriente por su **OUT−**. Si la celda regresara por otro lado, el límite
  de 3 A dejaría de funcionar.

Así, el D4184 enciende y apaga **el XL4016 completo**, y la celda va directo a la salida del
XL4016.

---

## Parte 9 — Bomba y tres ventiladores

**En paralelo** significa: todos los **rojos** juntos y todos los **negros** juntos.

| Cable | Va a |
|---|---|
| Rojo de la bomba, de los ventiladores 1 y 2 y del ventilador 60×60 | Torcidos juntos → **V+ #2** de la fuente |
| Negro de la bomba, de los ventiladores 1 y 2 y del ventilador 60×60 | Torcidos juntos → **LOAD** del D4184 #3 |
| Amarillo o azul de los ventiladores (si tienen) | Sin conectar, aislados con cinta |

Tuerce cada grupo de cuatro con un cable de calibre 18 y mete solo ese al tornillo. Entre los
cuatro consumen ≈ 1.0 A.

### Diodo 1N4007 (pegado a la bomba)

| Lado del diodo | Va a |
|---|---|
| Con **franja** | Rojo de la bomba |
| Sin franja | Negro de la bomba |

Si queda al revés, hace corto.

### Dirección del aire

| Ventilador | Flecha del costado |
|---|---|
| Entrada | Apunta **hacia dentro** del tubo |
| Salida | Apunta **hacia fuera** del tubo, rumbo a la cámara |

### Mangueras

| Desde | Hasta |
|---|---|
| Cubeta (succión) | Entrada de la bomba |
| Salida de la bomba | Waterblock A |
| Waterblock A | Waterblock B |
| Waterblock B | Cubeta (retorno), del lado opuesto a la succión |

✔ Llena la cubeta y revisa que no goteen las uniones.

---

## Parte 10 — Celdas Peltier (al final)

| Paso | Qué hacer |
|---|---|
| 1 | Capa **delgada** de pasta térmica en las dos caras, como una hoja de papel |
| 2 | Cara **con letras** (fría) contra el bloque de aluminio |
| 3 | Cara **lisa** (caliente) contra el waterblock |
| 4 | Aprieta parejo las cuatro esquinas, sin torcer: es cerámica y se rompe |
| 5 | Foam alrededor de los bordes |

| Celda | Rojo | Negro |
|---|---|---|
| A | OUT+ del XL4016 #1 | OUT− del XL4016 #1 |
| B | OUT+ del XL4016 #2 | OUT− del XL4016 #2 |

**Nunca conectes una celda directo a los 12 V de la fuente.** Intentaría jalar casi 10 A.

---

## Parte 11 — Programar y primer encendido

| Paso | Qué hacer | Cómo saber que quedó bien |
|---|---|---|
| 1 | Fuente **desenchufada**. Conecta el ESP32 a la computadora por el USB-C, sin la power bank | — |
| 2 | Borra la memoria del ESP32 y sube `FROSTER_Control` con **Partition Scheme → Minimal SPIFFS** | Termina sin errores |
| 3 | Abre el Monitor Serie a 115200 | Aparecen "Termometro bloque frio (GPIO18): detectado" y "OLED detectada"; la LCD muestra los datos del módulo y la OLED dice FROSTER; el LED parpadea en azul |
| 4 | Enchufa la fuente | Arrancan la bomba y los ventiladores |
| 5 | Espera | Las celdas encienden **después de 60 segundos**, solo si la cámara está sobre 28 °C |
| 6 | Mira en la pantalla la temperatura del **bloque frío** | **Debe bajar.** Si sube, desenchufa e invierte rojo y negro de esa celda |

Nunca conectes la power bank y la computadora al mismo tiempo en los dos USB de la placa.

---

## Voltajes y corrientes

| Componente | Voltaje | Corriente |
|---|---|---|
| Fuente | 12 V | ≈ 3.6 A de 5 A |
| XL4016 #1 y #2 | Entran 12 V, salen hasta 5.0 V | ≈ 1.3 A cada uno desde la fuente |
| Celda A y celda B | 3.7–4.6 V (lo ajusta el XL4016) | 3.0 A cada una |
| Bomba | 12 V | 0.5 A |
| Ventiladores 1 y 2 | 12 V | 0.14 A + 0.14 A = 0.28 A |
| Ventilador 60×60 (ACP06025S12M) | 12 V | 0.23 A |
| ESP32, sensores, pantalla, LED y buzzer | 5 V de la power bank | ≈ 0.3 A |

---

## Si algo falla

| Síntoma | Causa probable |
|---|---|
| No enciende nada de 12 V | Mide 12 V en la fuente y en el `+` de cada D4184 |
| Un XL4016 no enciende al querer ajustarlo | Su IN− ya va al LOAD: para ajustarlo, conéctalo directo a V− |
| Un D4184 se calienta | Su `+` no recibe 12 V |
| La pantalla prende sin letras | Gira el potenciómetro de atrás |
| El Monitor Serie dice "No respondio la OLED" | Revisa que SCL vaya a S del D17 y SDA a S del D16 |
| El Monitor Serie dice que un termómetro **NO APARECE** | Revisa sus tres cables, sobre todo DAT |
| Las celdas nunca encienden | Falla un sensor, o la cámara está bajo 28 °C |
| El bloque frío se calienta | Celda al revés: invierte rojo y negro |
| Un XL4016 se calienta tanto que no aguantas el dedo | Baja su límite a 2.5 A |
| El LED muestra colores al revés o el rojo no se apaga | Es de ánodo común: `LED_ANODO_COMUN` a `1` |
