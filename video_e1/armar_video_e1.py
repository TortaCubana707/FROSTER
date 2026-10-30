"""Une el video de la E1: titulo, animacion de Blender, fotos reales y la voz.

    blender -b --python armar_video_e1.py

Partes (24 fps, 1436 cuadros = 59.8 s):
  A  titulo sobre la foto del prototipo        voz A
  B  animacion de Blender (render_v2)           voz B
  C  fotos: fuente, potencia, indicadores       voz C
  D  foto de la LCD y la OLED en vivo           voz D
  E  cierre                                     voz E
"""
import bpy, os

AQUI = os.path.dirname(os.path.abspath(__file__))
CUADROS = os.path.join(AQUI, "cuadros")
ANIMACION = os.path.join(AQUI, "..", "blender", "render_v2")
VOZ = os.path.join(AQUI, "voz")
SALIDA = os.path.join(AQUI, "FROSTER_E1_video.mp4")

# (carpeta de cuadros, voz que arranca con la parte, cuadros de espera antes de la voz)
PARTES = [
    (os.path.join(CUADROS, "A_titulo"), "A", 8),
    (ANIMACION, "B", 10),
    (os.path.join(CUADROS, "C1_fuente"), "C", 6),
    (os.path.join(CUADROS, "C2_potencia"), None, 0),
    (os.path.join(CUADROS, "C3_indicadores"), None, 0),
    (os.path.join(CUADROS, "D_pantallas"), "D", 6),
    (os.path.join(CUADROS, "E_cierre"), "E", 2),
]

sc = bpy.context.scene
sc.render.fps = 24
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
ed = sc.sequence_editor_create()
tiras = ed.strips if hasattr(ed, "strips") else ed.sequences

inicio = 1
for k, (carpeta, voz, espera) in enumerate(PARTES):
    cuadros = sorted(f for f in os.listdir(carpeta) if f.startswith("cuadro_") and f.endswith(".png"))
    t = tiras.new_image(f"parte_{k}", os.path.join(carpeta, cuadros[0]), 1, inicio)
    for f in cuadros[1:]:
        t.elements.append(f)
    if voz:
        tiras.new_sound(f"voz_{voz}", os.path.join(VOZ, f"{voz}.mp3"), 2, inicio + espera)
    print(f"parte {k}: cuadros {inicio}-{inicio + len(cuadros) - 1}")
    inicio += len(cuadros)

sc.frame_start, sc.frame_end = 1, inicio - 1
im = sc.render.image_settings
try: im.media_type = "VIDEO"
except Exception: pass
im.file_format = "FFMPEG"
ff = sc.render.ffmpeg
ff.format, ff.codec, ff.constant_rate_factor = "MPEG4", "H264", "HIGH"
ff.audio_codec, ff.audio_bitrate, ff.audio_mixrate = "AAC", 192, 48000
ff.audio_channels = "STEREO"
sc.render.filepath = SALIDA
sc.render.use_file_extension = False
bpy.ops.render.render(animation=True)
print("VIDEO LISTO:", SALIDA, sc.frame_end, "cuadros =", round(sc.frame_end / 24, 1), "s")
