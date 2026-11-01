"""Diagramas de conexiones de FROSTER en Excalidraw.

    python3 generar_diagramas.py      -> escribe los .excalidraw junto a este archivo

Se renderizan a PNG con la skill excalidraw-diagram:
    cd ~/.claude/skills/excalidraw-diagram/references && uv run python render_excalidraw.py <archivo>

Convencion de cables: rojo = positivo, negro = tierra, azul = senal, naranja = salida del
reductor hacia la celda. Un punto lleno es una union; un "puente" (joroba) es un cruce sin
contacto.
"""
import json
import random
from pathlib import Path

AQUI = Path(__file__).parent
random.seed(12)

# ---- paleta (references/color-palette.md de la skill)
TITULO, SUBT, CUERPO, OSCURO = "#1e40af", "#3b82f6", "#64748b", "#374151"
ROJO, NEGRO, AZUL, NARANJA = "#dc2626", "#111827", "#2563eb", "#ea580c"
GRIS, VERDE_TIERRA = "#9ca3af", "#16a34a"
BASE = ("#dbeafe", "#1e40af")
ENTRADA = ("#fed7aa", "#c2410c")      # sensores
SALIDA = ("#a7f3d0", "#047857")       # pantalla, LED, buzzer, bomba, ventiladores
FUENTE = ("#fef3c7", "#b45309")
REDUCTOR = ("#ddd6fe", "#6d28d9")
MOSFET = ("#93c5fd", "#1e3a5f")
CELDA = ("#fee2e2", "#dc2626")
OPCIONAL = ("#f1f5f9", "#64748b")


class Lienzo:
    def __init__(self):
        self.el = []
        self.n = 0

    def _id(self, p):
        self.n += 1
        return f"{p}{self.n}"

    def _base(self, tipo, x, y, w, h, stroke, fill="transparent", sw=2, dashed=False):
        return {"type": tipo, "id": self._id(tipo[:3]), "x": x, "y": y, "width": w, "height": h,
                "strokeColor": stroke, "backgroundColor": fill, "fillStyle": "solid",
                "strokeWidth": sw, "strokeStyle": "dashed" if dashed else "solid",
                "roughness": 0, "opacity": 100, "angle": 0, "seed": random.randint(1, 10**8),
                "version": 1, "versionNonce": random.randint(1, 10**8), "isDeleted": False,
                "groupIds": [], "boundElements": None, "link": None, "locked": False}

    def texto(self, x, y, s, size=16, color=OSCURO, align="left"):
        lineas = s.split("\n")
        w = max(len(l) for l in lineas) * size * 0.6
        h = len(lineas) * size * 1.25
        if align == "center":
            x -= w / 2
        elif align == "right":
            x -= w
        e = self._base("text", x, y, w, h, color, sw=1)
        e.update({"text": s, "originalText": s, "fontSize": size, "fontFamily": 3,
                  "textAlign": align, "verticalAlign": "top", "containerId": None,
                  "lineHeight": 1.25})
        self.el.append(e)

    def caja(self, x, y, w, h, colores, dashed=False, sw=2):
        e = self._base("rectangle", x, y, w, h, colores[1], colores[0], sw=sw, dashed=dashed)
        e["roundness"] = {"type": 3}
        self.el.append(e)

    def punto(self, x, y, color=NEGRO, d=10):
        self.el.append(self._base("ellipse", x - d / 2, y - d / 2, d, d, color, color, sw=1))

    def linea(self, pts, color, sw=2, dashed=False, saltos=()):
        """Polilinea. `saltos` = puntos (x, y) sobre tramos horizontales donde se dibuja
        un puente: el cable pasa por encima de otro sin tocarlo."""
        out = [pts[0]]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            if y0 == y1:
                paso = 1 if x1 > x0 else -1
                for sx, sy in sorted(saltos, key=lambda s: s[0] * paso):
                    if sy == y0 and min(x0, x1) < sx < max(x0, x1):
                        out += [(sx - 9 * paso, y0), (sx - 6 * paso, y0 - 10),
                                (sx + 6 * paso, y0 - 10), (sx + 9 * paso, y0)]
            out.append((x1, y1))
        x0, y0 = out[0]
        rel = [[px - x0, py - y0] for px, py in out]
        xs, ys = [p[0] for p in rel], [p[1] for p in rel]
        e = self._base("line", x0, y0, max(xs) - min(xs), max(ys) - min(ys), color, sw=sw,
                       dashed=dashed)
        e.update({"points": rel, "lastCommittedPoint": None, "startBinding": None,
                  "endBinding": None, "startArrowhead": None, "endArrowhead": None,
                  "roundness": None})
        self.el.append(e)

    def resistencia(self, x, y, etiqueta):
        """Resistencia dibujada encima de un cable horizontal, centrada en (x, y)."""
        self.caja(x - 26, y - 9, 52, 18, ("#ffffff", NEGRO), sw=2)
        self.texto(x, y - 30, etiqueta, 14, NEGRO, "center")

    def guardar(self, nombre):
        data = {"type": "excalidraw", "version": 2, "source": "https://excalidraw.com",
                "elements": self.el,
                "appState": {"viewBackgroundColor": "#ffffff", "gridSize": 20}, "files": {}}
        ruta = AQUI / nombre
        ruta.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
        print("escrito", ruta, len(self.el), "elementos")


def leyenda(c, x, y, items):
    for color, texto, tipo in items:
        if tipo == "linea":
            c.linea([(x, y + 10), (x + 46, y + 10)], color, sw=3)
            c.texto(x + 56, y, texto, 15, OSCURO)
        else:
            c.punto(x + 23, y + 10, color, 11)
            c.texto(x + 56, y, texto, 15, OSCURO)
        x += 56 + len(texto) * 9 + 40


# =============================================================================
#  DIAGRAMA 1 · LADO DE CONTROL
# =============================================================================
def control():
    c = Lienzo()
    c.texto(40, 24, "FROSTER · Conexiones del lado de control", 34, TITULO)
    c.texto(40, 72, "Todo lo que va a la placa del ESP32. Se alimenta por el USB-C (power bank). "
                    "Jumper de la placa en 5V.", 18, CUERPO)
    leyenda(c, 40, 110, [(ROJO, "V = +5V", "linea"), (NEGRO, "G = tierra", "linea"),
                         (AZUL, "S = señal", "linea"), (NEGRO, "punto = pin o unión", "punto")])

    # ---- placa
    BX, BY, BW, BH = 760, 180, 420, 1130
    c.caja(BX, BY, BW, BH, BASE, sw=3)
    c.texto(BX + BW / 2, 540, "ESP32 DevKit\nen la placa\nde expansión", 26, TITULO, "center")
    c.texto(BX + BW / 2, 650, "jumper amarillo:\nV con 5V", 18, OSCURO, "center")
    c.texto(BX + 16, BY + 12, "fila D23 … D4", 15, CUERPO)
    c.texto(BX + BW - 16, BY + 12, "fila D13 … D32", 15, CUERPO, "right")
    c.caja(BX + BW / 2 - 70, BY + BH - 22, 140, 44, ("#ffffff", NEGRO))
    c.texto(BX + BW / 2, BY + BH - 10, "USB-C", 18, NEGRO, "center")

    def fila(nombre, y, lado):
        """Fila G·V·S de la placa: devuelve {G, V, S: (x, y)}."""
        x = BX if lado == "izq" else BX + BW
        pines = {}
        for i, letra in enumerate("GVS"):
            py = y + 22 * i
            c.punto(x, py, NEGRO, 10)
            if lado == "izq":
                c.texto(x + 12, py - 9, letra, 15, OSCURO)
            else:
                c.texto(x - 12, py - 9, letra, 15, OSCURO, "right")
            pines[letra] = (x, py)
        if lado == "izq":
            c.texto(x + 34, y + 9, nombre, 20, TITULO)
        else:
            c.texto(x - 34, y + 9, nombre, 20, TITULO, "right")
        return pines

    def componente(x, y, w, h, colores, titulo, pines, lado="der", dashed=False):
        """Caja con pines en un costado. pines = [(etiqueta, y)]. Devuelve {etiqueta: (x, y)}."""
        c.caja(x, y, w, h, colores, dashed=dashed)
        c.texto(x + 14, y + 10, titulo, 17, OSCURO)
        out = {}
        px = x + w if lado == "der" else x
        for etq, py in pines:
            c.punto(px, py, NEGRO, 10)
            if lado == "der":
                c.texto(px - 12, py - 9, etq, 15, OSCURO, "right")
            else:
                c.texto(px + 12, py - 9, etq, 15, OSCURO)
            out[etq] = (px, py)
        return out

    def cable(a, b, color, saltos=()):
        c.linea([a, b], color, sw=3, saltos=saltos)

    # ---- columna izquierda (fila D23 ... D4, en el orden de la placa)
    d23 = fila("D23", 230, "izq")
    d22 = fila("D22", 360, "izq")
    d21 = fila("D21", 470, "izq")
    d18 = fila("D18", 640, "izq")
    d5 = fila("D5", 790, "izq")
    d17 = fila("D17", 940, "izq")
    d16 = fila("D16", 1050, "izq")
    d4 = fila("D4", 1180, "izq")

    # Buzzer pasivo de 3 pines: la señal audible del circuito
    bz = componente(180, 205, 280, 100, SALIDA, "Buzzer pasivo\n3 pines",
                    [("GND", d23["G"][1]), ("VCC", d23["V"][1]), ("S", d23["S"][1])])
    cable(bz["GND"], d23["G"], NEGRO)
    cable(bz["VCC"], d23["V"], ROJO)
    cable(bz["S"], d23["S"], AZUL)

    # LCD sola en el bus I2C principal (21 = SDA, 22 = SCL)
    lcd = componente(180, 340, 280, 200, SALIDA, "Pantalla LCD\n16×2 I2C",
                     [("SCL", 404), ("GND", 470), ("VCC", 492), ("SDA", 514)])
    cable(lcd["SCL"], d22["S"], AZUL)
    cable(lcd["GND"], d21["G"], NEGRO)
    cable(lcd["VCC"], d21["V"], ROJO)
    cable(lcd["SDA"], d21["S"], AZUL)
    c.texto(BX - 16, 332, "G y V del D22: sin usar", 13, CUERPO, "right")

    # Solo un DS18B20: dentro del bloque frío (D18). El agua ya no lleva termómetro.
    c.caja(180, 765, 280, 100, OPCIONAL, dashed=True)
    c.texto(194, 777, "D5 libre", 17, OSCURO)
    c.texto(194, 803, "Sin termómetro en el agua: la\nfalta de hielo se deduce del\nbloque frío.", 13, CUERPO)
    for pines_fila, y, titulo in ((d18, 615, "Termómetro\ndentro del bloque frío"),
                                  (d4, 1155, "DHT22\ndentro de la caja")):
        g, v, s = pines_fila["G"][1], pines_fila["V"][1], pines_fila["S"][1]
        etq = ("GND", "VCC", "DATA") if "DHT" in titulo else ("GND", "VCC", "DAT")
        comp = componente(180, y, 280, 100, ENTRADA, titulo, [(etq[0], g), (etq[1], v), (etq[2], s)])
        cable(comp[etq[0]], pines_fila["G"], NEGRO)
        cable(comp[etq[1]], pines_fila["V"], ROJO)
        cable(comp[etq[2]], pines_fila["S"], AZUL)

    # OLED en su PROPIO bus I2C (16 = SDA, 17 = SCL): no comparte nada con la LCD
    ol = componente(180, 915, 280, 200, SALIDA, "OLED 0.96\" I2C\nbus propio",
                    [("GND", 940), ("VCC", 962), ("SCL", 984), ("SDA", 1094)])
    cable(ol["GND"], d17["G"], NEGRO)
    cable(ol["VCC"], d17["V"], ROJO)
    cable(ol["SCL"], d17["S"], AZUL)
    cable(ol["SDA"], d16["S"], AZUL)
    c.texto(BX - 16, 1112, "G y V del D16: sin usar", 13, CUERPO, "right")

    # ---- columna derecha (fila D13 ... D32)
    d13 = fila("D13", 230, "der")
    d27 = fila("D27", 450, "der")
    d26 = fila("D26", 580, "der")
    d25 = fila("D25", 710, "der")
    d33 = fila("D33", 850, "der")
    d32 = fila("D32", 960, "der")

    STUB = 1300
    c.texto(STUB - 100, 330, "Los D4184 tienen optoacoplador:\nno hace falta unir tierras con\nla fuente de 12V.", 14, CUERPO)
    for f, n, que in ((d27, 3, "bomba y\nventiladores"), (d26, 2, "celda B"), (d25, 1, "celda A")):
        cable(f["S"], (STUB, f["S"][1]), AZUL)
        cable(f["G"], (STUB, f["G"][1]), NEGRO)
        c.punto(STUB, f["S"][1], AZUL, 9)
        c.punto(STUB, f["G"][1], NEGRO, 9)
        c.texto(STUB + 10, f["G"][1] - 9, f"→ GND del D4184 #{n}", 14, NEGRO)
        c.texto(STUB + 10, f["S"][1] - 9, f"→ PWM del D4184 #{n}", 14, AZUL)
        c.texto(STUB + 10, f["V"][1] - 9, f"   ({que.replace(chr(10), ' ')})", 13, CUERPO)
    c.texto(STUB + 10, 785, "(siguen en el diagrama\nde potencia)", 13, CUERPO)

    led = componente(1600, 840, 280, 210, SALIDA, "LED RGB · cátodo común",
                     [("G verde", 894), ("B azul", 934), ("común", 960), ("R rojo", 1004)],
                     lado="izq")
    # B: sube por un canal propio para no cruzar los cables del D4184
    c.linea([d13["S"], (1540, 274), (1540, 934), led["B azul"]], AZUL, sw=3)
    c.resistencia(1420, 274, "220 Ω")
    c.linea([d33["S"], led["G verde"]], AZUL, sw=3, saltos=[(1540, 894)])
    c.resistencia(1420, 894, "220 Ω")
    cable(d32["G"], led["común"], NEGRO)
    cable(d32["S"], led["R rojo"], AZUL)
    c.resistencia(1420, 1004, "220 Ω")
    c.texto(1600, 1062, "Si es de ánodo común: la pata larga va\nal pin 3.3V del jumper y en el código\nLED_ANODO_COMUN = 1", 13, CUERPO)
    c.texto(1600, 800, "la pata más larga = común", 13, CUERPO)

    # ---- alimentacion
    PBY = BY + BH + 80
    c.caja(890, PBY, 160, 74, SALIDA)
    c.texto(970, PBY + 12, "Power bank\n5V", 18, OSCURO, "center")
    c.linea([(970, PBY), (970, BY + BH + 22)], ROJO, sw=4)
    c.texto(984, PBY - 44, "cable USB-C", 14, CUERPO)
    c.texto(1080, PBY, "Para programar: quita la power bank y conecta\nla computadora en el MISMO USB-C. Nunca los dos.\nNo uses el jack DC de la placa.", 14, CUERPO)

    # ---- que muestra cada pantalla
    OX, OY = 40, PBY + 130
    c.caja(OX, OY, 1840, 390, ("#f8fafc", CUERPO))
    c.texto(OX + 20, OY + 14, "Qué muestra cada pantalla (ejemplos)", 19, TITULO)

    # OLED: lo que decide el control
    c.texto(OX + 20, OY + 46, "OLED · lo que decide el control", 16, OSCURO)
    PX, PY, PW, PH = OX + 20, OY + 76, 384, 192           # 128×64 a escala ×3
    c.caja(PX, PY, PW, PH, ("#0b1220", "#334155"), sw=3)
    CI = "#5eead4"
    c.texto(PX + 10, PY + 8, "N2 28.3", 30, CI)
    c.texto(PX + 262, PY + 6, "°C", 16, CI)
    c.caja(PX + 296, PY + 6, 22, 22, (CI, CI), sw=1)
    c.texto(PX + 301, PY + 7, "A", 16, "#0b1220")
    c.texto(PX + 330, PY + 7, "B", 16, CI)
    c.caja(PX + 352, PY + 6, 22, 22, (CI, CI), sw=1)
    c.texto(PX + 356, PY + 7, "~", 16, "#0b1220")
    c.texto(PX + 262, PY + 30, "↑ obj28", 16, CI)
    c.linea([(PX + 6, PY + 112), (PX + PW - 6, PY + 112)], CI, sw=1, dashed=True)
    curva = [(70, 150), (100, 146), (130, 128), (160, 112), (200, 100), (240, 96), (280, 104),
             (320, 112), (370, 112)]
    c.linea([(PX + x + 6, PY + y) for x, y in curva], CI, sw=2)
    c.texto(PX + 10, PY + 58, "29.4", 13, CI)
    c.texto(PX + 10, PY + 138, "27.1", 13, CI)
    c.texto(PX + 10, PY + 168, "Sobre objetivo", 16, CI)
    c.texto(PX + PW + 20, PY - 2, "Arriba: nivel y cámara.\nA, B, ~ resaltados =\ncelda A, celda B, bomba.\n"
            "Flecha: tendencia.\nobj: objetivo.\nGráfica: últimos ~4 min;\npunteada = objetivo.\n"
            "Abajo: por qué decidió\n(parpadea si hay alerta).", 13, OSCURO)

    # LCD: el modulo, el uso y la red
    LX = OX + 860
    c.texto(LX, OY + 46, "LCD 16×2 · el módulo, el uso y la red (cambia cada 3 s)", 16, OSCURO)
    vistas = [("1 · el módulo", "Blq  5.5 Ag  8.1", "Roc 14.2 Hum 55%"),
              ("2 · uso de celdas", "Uso celdas (h)", "A  0.53 B  0.21"),
              ("3 · la red", "WiFi -62dBm FB", "192.168.1.50"),
              ("si hay alerta", "** ALERTA **", "Bloque congelado")]
    for i, (nombre, l1, l2) in enumerate(vistas):
        x = LX + (i % 2) * 470
        y = OY + 86 + (i // 2) * 140
        c.caja(x, y, 240, 78, ("#1d4ed8", "#1e3a8a"), sw=3)
        c.texto(x + 12, y + 12, l1, 20, "#e0f2fe")
        c.texto(x + 12, y + 42, l2, 20, "#e0f2fe")
        c.texto(x + 256, y + 28, nombre, 14, OSCURO)
    c.texto(LX, OY + 360, "Blq = bloque frío · Ag = agua del waterblock · Roc = punto de rocío · FB = Firebase conectado", 13, CUERPO)
    c.texto(OX + 20, OY + 300, "Si la OLED no está conectada, el sistema sigue igual con la LCD.", 14, CUERPO)
    c.texto(OX + 20, OY + 324, "Guíate por las letras de cada pantalla: la OLED trae GND·VCC·SCL·SDA\ny la LCD GND·VCC·SDA·SCL.", 14, ROJO)

    c.guardar("FROSTER_conexiones_control.excalidraw")


# =============================================================================
#  DIAGRAMA 2 · LADO DE POTENCIA
# =============================================================================
def potencia():
    c = Lienzo()
    c.texto(40, 24, "FROSTER · Conexiones del lado de potencia", 34, TITULO)
    c.texto(40, 72, "Fuente de 12V 5A. Desenchúfala siempre mientras conectas. "
                    "Las celdas se conectan al final.", 18, CUERPO)
    leyenda(c, 40, 110, [(ROJO, "+12V", "linea"), (NEGRO, "tierra (−)", "linea"),
                         (NARANJA, "salida del XL4016 (~4V)", "linea"), (AZUL, "señal del ESP32", "linea"),
                         (NEGRO, "punto = unión", "punto")])
    c.texto(40, 140, "Joroba en un cable = se cruzan sin tocarse.", 15, CUERPO)

    def caja_pines(x, y, w, h, colores, titulo, izq=(), der=(), abajo=(), sub=None, tam=18):
        c.caja(x, y, w, h, colores)
        c.texto(x + w / 2, y + 12, titulo, tam, OSCURO, "center")
        if sub:
            c.texto(x + w / 2, y + 36, sub, 13, CUERPO, "center")
        p = {}
        for etq, py in izq:
            c.punto(x, py, NEGRO, 10); c.texto(x + 12, py - 9, etq, 15, OSCURO); p[etq] = (x, py)
        for etq, py in der:
            c.punto(x + w, py, NEGRO, 10); c.texto(x + w - 12, py - 9, etq, 15, OSCURO, "right"); p[etq] = (x + w, py)
        for etq, px in abajo:
            c.punto(px, y + h, NEGRO, 10); c.texto(px, y + h - 24, etq, 14, OSCURO, "center"); p[etq] = (px, y + h)
        return p

    def d4184(y, n, que):
        """Modulo como el de la foto: bornera azul (+ LOAD -) a la derecha,
        bornera verde (PWM GND, aislada) abajo."""
        p = caja_pines(620, y, 210, 170, MOSFET, f"D4184 #{n}", sub=que,
                       der=[("+", y + 40), ("LOAD", y + 80), ("−", y + 120)],
                       abajo=[("PWM", 680), ("GND", 770)])
        c.texto(725, y + 104, "verde ↓", 12, CUERPO, "center")
        return p

    def senal(m, pin):
        for etq, color, letra in (("PWM", AZUL, "S"), ("GND", NEGRO, "G")):
            x, y = m[etq]
            c.linea([(x, y), (x, y + 38)], color, sw=3)
            c.punto(x, y + 38, color, 9)
            c.texto(x, y + 44, f"{letra} del {pin}", 14, color, "center")

    # ---- clavija y fuente
    c.caja(40, 400, 150, 140, ("#f8fafc", NEGRO))
    c.texto(115, 415, "Clavija\n127V", 18, OSCURO, "center")
    fu = caja_pines(300, 200, 220, 1010, FUENTE, "Fuente", sub="12V 5A 60W",
                    izq=[("L", 440), ("N", 480), ("⏚", 520)],
                    der=[("V+ #1", 260), ("V− #1", 300), ("V+ #2", 990), ("V− #2", 1070)])
    for (etq, color, nombre) in (("L", NEGRO, "negro"), ("N", GRIS, "blanco"), ("⏚", VERDE_TIERRA, "verde")):
        y = fu[etq][1]
        c.linea([(190, y), fu[etq]], color, sw=4)
        c.texto(200, y - 22, nombre, 13, color if color != GRIS else CUERPO)
    c.texto(320, 600, "cierra la tapa\ntransparente\nde L, N y ⏚", 14, CUERPO)
    c.texto(320, 700, "mide 12.0V\nentre V+ y V−\nantes de seguir", 14, CUERPO)

    # ---- troncos de 12V para las dos celdas
    c.linea([fu["V+ #1"], (560, 260)], ROJO, sw=3)
    c.linea([(560, 170), (560, 570)], ROJO, sw=3)
    c.punto(560, 260, ROJO, 11)
    c.linea([fu["V− #1"], (580, 300)], NEGRO, sw=3, saltos=[(560, 300)])
    c.linea([(580, 300), (580, 860)], NEGRO, sw=3)
    c.punto(580, 300, NEGRO, 11)

    def rama_celda(y0, n, letra, pin):
        """D4184 en el negativo de la ENTRADA del XL4016 (a 12V el MOSFET enciende completo)
        y la celda directo en la salida del XL4016 (asi funciona su limite de corriente)."""
        m = d4184(y0 + 30, n, f"celda {letra}")
        xl = caja_pines(960, y0 + 10, 220, 150, REDUCTOR, f"XL4016 #{n}", sub="CV 5.0V · CC 3.0A",
                        izq=[("IN+", y0 + 70), ("IN−", y0 + 110)],
                        der=[("OUT+", y0 + 70), ("OUT−", y0 + 110)])
        ce = caja_pines(1320, y0 + 20, 300, 130, CELDA, f"Celda Peltier {letra}", sub="TEC1-12710 · 3.0A",
                        izq=[("rojo", y0 + 70), ("negro", y0 + 110)])
        c.texto(1640, y0 + 30, "cara con letras = fría\n→ contra el bloque de aluminio\ncara lisa → waterblock", 14, CUERPO)
        # +12V: tronco -> IN+ del XL4016 y + del D4184
        ytop = y0
        c.linea([(560, ytop), (900, ytop), (900, y0 + 70), xl["IN+"]], ROJO, sw=3,
                saltos=[(580, ytop)] if ytop > 300 else [])
        c.punto(560, ytop, ROJO, 9) if ytop != 170 else None
        c.linea([(900, y0 + 70), (870, y0 + 70)], ROJO, sw=3)
        c.linea([(870, y0 + 70), m["+"]], ROJO, sw=3)
        c.punto(900, y0 + 70, ROJO, 9)
        # LOAD -> IN- del XL4016
        c.linea([m["LOAD"], (920, y0 + 110), xl["IN−"]], NEGRO, sw=3)
        # - del D4184 -> tronco de tierra (por debajo de las etiquetas de senal)
        ybajo = y0 + 290
        c.linea([m["−"], (860, y0 + 150), (860, ybajo), (580, ybajo)], NEGRO, sw=3)
        c.punto(580, ybajo, NEGRO, 9)
        # salida a la celda
        c.linea([xl["OUT+"], ce["rojo"]], NARANJA, sw=3)
        c.linea([xl["OUT−"], ce["negro"]], NEGRO, sw=3)
        senal(m, pin)
        return m

    rama_celda(170, 1, "A", "D25")
    rama_celda(570, 2, "B", "D26")
    c.texto(1320, 340, "La celda regresa por OUT− del XL4016:\nasí funciona su límite de 3.0A.", 14, CUERPO)
    c.texto(1320, 740, "Nunca conectes una celda directo\na los 12V de la fuente.", 14, ROJO)

    # ---- bomba y ventiladores
    m3 = d4184(940, 3, "bomba + ventil.")
    c.linea([fu["V+ #2"], (600, 990), (600, 900), (880, 900), (880, 980), m3["+"]], ROJO, sw=3)
    c.punto(880, 900, ROJO, 10)
    c.linea([fu["V− #2"], (595, 1070), (595, 1200), (870, 1200), (870, 1060), m3["−"]], NEGRO, sw=3)
    senal(m3, "D27")

    cargas = [("Bomba de diafragma", "12V · 0.5A", 980), ("Ventilador 1", "12V · 0.14A", 1110),
              ("Ventilador 2", "12V · 0.14A", 1240), ("Ventilador 60×60", "ACP06025 · 12V · 0.23A", 1370)]
    # bus rojo (x=1290) sale del mismo +12V; bus negro (x=1340) sale de LOAD
    c.linea([(880, 900), (1290, 900), (1290, 1370)], ROJO, sw=3)
    c.linea([m3["LOAD"], (1340, 1020)], NEGRO, sw=3, saltos=[(1290, 1020)])
    c.linea([(1340, 1010), (1340, 1400)], NEGRO, sw=3)
    c.punto(1340, 1020, NEGRO, 11)
    for titulo, sub, y in cargas:
        p = caja_pines(1440, y - 50, 260, 116, SALIDA, titulo, izq=[("rojo", y), ("negro", y + 30)])
        c.texto(1690, y + 6, sub, 13, CUERPO, "right")
        c.linea([(1290, y), p["rojo"]], ROJO, sw=3, saltos=[(1340, y)] if y > 1010 else [])
        c.linea([(1340, y + 30), p["negro"]], NEGRO, sw=3)
        c.punto(1290, y, ROJO, 9)
        c.punto(1340, y + 30, NEGRO, 9)
    c.caja(1386, 985, 22, 20, ("#ffffff", NEGRO))
    c.caja(1386, 985, 22, 5, (NEGRO, NEGRO))
    c.punto(1397, 980, ROJO, 8)
    c.punto(1397, 1010, NEGRO, 8)
    c.texto(1720, 960, "Diodo 1N4007 entre los\ncables de la bomba:\nfranja → rojo (+)", 14, NEGRO)
    c.texto(1720, 1100, "Rojos: al +12V.\nNegros: a LOAD del D4184 #3.\nLos 4 en paralelo: ≈1.0A en total.", 14, CUERPO)
    c.texto(1720, 1240, "Flechas de los ventiladores\nen la misma dirección", 14, CUERPO)

    # ---- como es el modulo
    c.caja(960, 1460, 760, 150, ("#f8fafc", CUERPO))
    c.texto(980, 1474, "Así es cada D4184 (como el de tu foto):", 16, TITULO)
    c.texto(980, 1504, "· bornera azul:  +  = +12V   ·   LOAD = negativo de lo que enciende   ·   − = tierra de 12V\n"
                       "· bornera verde: PWM ← S del ESP32   ·   GND ← G del ESP32\n"
                       "· la verde está aislada (optoacoplador): NO se une con la tierra de 12V\n"
                       "· su + necesita 6V o más: por eso va en los 12V, antes del XL4016", 14, OSCURO)

    # ---- ajustes previos
    c.caja(40, 1460, 860, 170, ("#f8fafc", CUERPO))
    c.texto(60, 1474, "Antes de conectar las celdas (cada módulo sin carga):", 16, TITULO)
    c.texto(60, 1504, "· Fuente: 12.0V entre V+ y V−\n· XL4016 #1 y #2: voltaje 5.0V y límite 3.0A (multímetro en 10A)\n"
                      "· Regresa la punta roja del multímetro a VΩ\n· Ajústalo ANTES de poner su D4184: mientras, su IN− va directo a V−;\n  ya ajustado, pasa ese cable a LOAD", 14, OSCURO)

    # ---- foco: carga de calor de la camara, a 127V y SEPARADO de todo lo demas
    FY = 1680
    c.caja(40, FY, 1680, 330, ("#fffbeb", "#b45309"), sw=2, dashed=True)
    c.texto(60, FY + 14, "Foco amarillo · carga de calor de la cámara · 127V · va aparte", 19, "#b45309")
    c.texto(60, FY + 42, "No se conecta al ESP32 ni a la fuente de 12V. Simula la computadora que el módulo tiene que enfriar.", 14, CUERPO)
    c.caja(60, FY + 90, 150, 120, ("#f8fafc", NEGRO))
    c.texto(135, FY + 105, "Clavija\n127V\n(a la pared)", 16, OSCURO, "center")
    sw = caja_pines(380, FY + 80, 170, 80, ("#f1f5f9", NEGRO), "Apagador\n(opcional)", izq=[("", FY + 130)], der=[("", FY + 130)])
    so = caja_pines(720, FY + 80, 240, 150, ("#ffffff", NEGRO), "Base del foco",
                    izq=[("negro → centro", FY + 130), ("blanco → rosca", FY + 190)])
    c.linea([(210, FY + 130), sw[""] if False else (380, FY + 130)], NEGRO, sw=4)
    c.texto(222, FY + 108, "fase · pata delgada", 13, NEGRO)
    c.linea([(550, FY + 130), so["negro → centro"]], NEGRO, sw=4)
    c.linea([(210, FY + 190), so["blanco → rosca"]], GRIS, sw=4)
    c.texto(222, FY + 196, "neutro · pata ancha", 13, CUERPO)
    c.punto(380, FY + 130, NEGRO, 10); c.punto(550, FY + 130, NEGRO, 10)
    # foco
    c.el.append({**c._base("ellipse", 1010, FY + 85, 110, 110, "#b45309", "#fde68a", sw=2)})
    c.caja(1045, FY + 192, 40, 30, ("#d1d5db", "#6b7280"))
    c.texto(1065, FY + 128, "foco", 16, "#b45309", "center")
    c.linea([(960, FY + 155), (1045, FY + 205)], CUERPO, sw=1, dashed=True)
    c.texto(1160, FY + 84,
            "· Incandescente o halógeno: un foco LED casi no calienta.\n"
            "· 25 a 40 W: el módulo le gana con 25 W y llega a su\n"
            "  límite con 40 W. Con 60 W o más la cámara siempre sube.\n"
            "· El apagador corta la FASE (negro), nunca el neutro.\n"
            "· Comprueba con continuidad: negro = centro, blanco = rosca.\n"
            "· Sin apagador: une directo y apaga desenchufando.\n"
            "· Base de plástico: no pases los watts que trae impresos.\n"
            "· Cables por arriba de la cámara, lejos de agua y bandeja.\n"
            "· Empalmes con termorretráctil, nunca solo con cinta.\n"
            "· Tapa el DHT22 de la luz directa del foco (cartón).", 14, OSCURO)

    c.guardar("FROSTER_conexiones_potencia.excalidraw")


if __name__ == "__main__":
    control()
    potencia()
