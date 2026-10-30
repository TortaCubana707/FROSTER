"""Genera los cuadros (1280x720, 24 fps) de las partes del video E1 hechas con las fotos reales.

    python3 gen_diapositivas.py

Cada parte queda en cuadros/<parte>/cuadro_0001.png ... y armar_video_e1.py las une con la
animacion de Blender y la voz.
"""
import math, os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

AQUI = os.path.dirname(os.path.abspath(__file__))
FOTOS = os.path.join(AQUI, "..", "evidencias", "E1")
W, H, FPS = 1280, 720, 24
NEGRITA = "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf"
NORMAL = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"
CIAN, BLANCO, GRIS = (88, 200, 255), (240, 244, 248), (170, 180, 192)

# Duraciones en cuadros: cada parte dura lo que su voz, mas un respiro
PARTES = {
    "A_titulo": 152,
    "C1_fuente": 128, "C2_potencia": 128, "C3_indicadores": 128,
    "D_pantallas": 248,
    "E_cierre": 100,
}

DIAPOSITIVAS = {
    "C1_fuente": ("01_fuente_y_conexion_a_la_toma.jpg", "PROTOTIPO REAL", "Alimentación",
                  ["Fuente de 12 V y 5 A", "Una rama para las celdas", "Otra para bomba y ventiladores"]),
    "C2_potencia": ("03_prototipo_completo.jpg", "PROTOTIPO REAL", "Potencia",
                    ["2 × XL4016: 5 V y 3 A por celda", "3 × MOSFET D4184", "Pines del ESP32: GPIO 25, 26 y 27"]),
    "C3_indicadores": ("02_modulo_D4184_XL4016_y_LED_RGB.jpg", "PROTOTIPO REAL", "Indicadores",
                       ["LED RGB: verde N1 · cian N2 · azul N3", "Buzzer pasivo en GPIO 23",
                        "Melodía al cambiar de nivel"]),
    "D_pantallas": ("04_LCD_WiFi_y_OLED_nivel.jpg", "EN VIVO POR WI-FI", "Pantallas y nube",
                    ["LCD: Wi-Fi -50 dBm · IP 192.168.100.24", "OLED: nivel, temperatura y gráfica",
                     "Firebase en tiempo real"]),
}


def fuente(ruta, tam):
    return ImageFont.truetype(ruta, tam)


def suave(x):
    """0..1 con arranque y frenado suaves."""
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def fondo_desenfocado(foto):
    esc = max(W / foto.width, H / foto.height)
    f = foto.resize((int(foto.width * esc), int(foto.height * esc)))
    x, y = (f.width - W) // 2, (f.height - H) // 2
    f = f.crop((x, y, x + W, y + H)).filter(ImageFilter.GaussianBlur(28))
    return Image.blend(f, Image.new("RGB", (W, H), (8, 12, 18)), 0.72)


def tarjeta_foto(foto, t, alto=640, ancho=430):
    """Recorte vertical de la foto con un acercamiento lento (efecto Ken Burns)."""
    zoom = 1.0 + 0.07 * t
    esc = max(ancho / foto.width, alto / foto.height) * zoom
    f = foto.resize((int(foto.width * esc), int(foto.height * esc)), Image.LANCZOS)
    x, y = (f.width - ancho) // 2, int((f.height - alto) * (0.5 - 0.1 * t))
    f = f.crop((x, y, x + ancho, y + alto))
    mascara = Image.new("L", (ancho, alto), 0)
    ImageDraw.Draw(mascara).rounded_rectangle((0, 0, ancho - 1, alto - 1), 28, fill=255)
    return f, mascara


def texto_aparece(lienzo, xy, cuerpo, tipo, color, avance):
    """Texto que entra desde abajo y aparece (avance 0..1)."""
    a = suave(avance)
    if a <= 0:
        return
    capa = Image.new("RGBA", lienzo.size, (0, 0, 0, 0))
    ImageDraw.Draw(capa).text((xy[0], xy[1] + (1 - a) * 18), cuerpo, font=tipo,
                              fill=(*color, int(255 * a)))
    lienzo.alpha_composite(capa)


def diapositiva(nombre, n):
    archivo, etiqueta, titulo, puntos = DIAPOSITIVAS[nombre]
    foto = Image.open(os.path.join(FOTOS, archivo)).convert("RGB")
    fondo = fondo_desenfocado(foto).convert("RGBA")
    f_tag, f_tit, f_pto = fuente(NEGRITA, 22), fuente(NEGRITA, 54), fuente(NORMAL, 30)
    for i in range(n):
        t = i / (n - 1)
        img = fondo.copy()
        recorte, mascara = tarjeta_foto(foto, t)
        sombra = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(sombra).rounded_rectangle((92, 52, 92 + 430, 52 + 640), 28, fill=(0, 0, 0, 150))
        img.alpha_composite(sombra.filter(ImageFilter.GaussianBlur(14)))
        img.paste(recorte, (80, 40), mascara)
        seg = i / FPS
        d = ImageDraw.Draw(img)
        # etiqueta tipo "chip"
        if seg > 0.15:
            ancho = d.textlength(etiqueta, font=f_tag) + 36
            d.rounded_rectangle((590, 150, 590 + ancho, 192), 21, outline=CIAN, width=2)
            d.text((608, 157), etiqueta, font=f_tag, fill=CIAN)
        texto_aparece(img, (588, 212), titulo, f_tit, BLANCO, (seg - 0.2) / 0.5)
        for k, p in enumerate(puntos):
            inicio = 0.9 + k * 0.9
            y = 320 + k * 70
            a = suave((seg - inicio) / 0.4)
            if a > 0:
                ImageDraw.Draw(img).ellipse((592, y + 14, 604, y + 26), fill=(*CIAN, int(255 * a)))
            texto_aparece(img, (622, y), p, f_pto, BLANCO, (seg - inicio) / 0.4)
        yield img.convert("RGB")


def titulo(n, archivo="03_prototipo_completo.jpg", grande="FROSTER",
           sub="Enfriamiento termoeléctrico inteligente", chico="ESP32 · Celdas Peltier · Firebase"):
    foto = Image.open(os.path.join(FOTOS, archivo)).convert("RGB")
    fondo = fondo_desenfocado(foto).convert("RGBA")
    f_g, f_s, f_c = fuente(NEGRITA, 128), fuente(NEGRITA, 40), fuente(NORMAL, 28)
    for i in range(n):
        seg = i / FPS
        img = fondo.copy()
        d = ImageDraw.Draw(img)
        a = suave(seg / 0.6)
        esc = 0.85 + 0.15 * a
        capa = Image.new("RGBA", (W, 200), (0, 0, 0, 0))
        dc = ImageDraw.Draw(capa)
        ancho = dc.textlength(grande, font=f_g)
        dc.text(((W - ancho) / 2, 10), grande, font=f_g, fill=(*BLANCO, int(255 * a)))
        cx = int(W * esc)
        capa = capa.resize((cx, int(200 * esc)))
        img.alpha_composite(capa, ((W - cx) // 2, int(200 + (1 - esc) * 100)))
        for cuerpo, tipo, color, y, inicio in ((sub, f_s, CIAN, 420, 0.6), (chico, f_c, GRIS, 480, 1.0)):
            ancho = d.textlength(cuerpo, font=tipo)
            texto_aparece(img, ((W - ancho) / 2, y), cuerpo, tipo, color, (seg - inicio) / 0.5)
        # la ultima media segundo se funde a negro para entrar a la animacion
        negro = suave((seg - (n / FPS - 0.5)) / 0.5)
        if negro > 0:
            img = Image.blend(img, Image.new("RGBA", (W, H), (0, 0, 0, 255)), negro)
        yield img.convert("RGB")


def guardar(nombre, cuadros):
    carpeta = os.path.join(AQUI, "cuadros", nombre)
    os.makedirs(carpeta, exist_ok=True)
    for i, img in enumerate(cuadros, 1):
        img.save(os.path.join(carpeta, f"cuadro_{i:04d}.png"), compress_level=1)
    print(nombre, i, "cuadros")


if __name__ == "__main__":
    guardar("A_titulo", titulo(PARTES["A_titulo"]))
    for nombre in ("C1_fuente", "C2_potencia", "C3_indicadores", "D_pantallas"):
        guardar(nombre, diapositiva(nombre, PARTES[nombre]))
    guardar("E_cierre", titulo(PARTES["E_cierre"], "04_LCD_WiFi_y_OLED_nivel.jpg", "FROSTER",
                                "Frío inteligente, controlado desde la nube",
                                "E1 · Prototipo físico IoT y firmware ESP32"))
