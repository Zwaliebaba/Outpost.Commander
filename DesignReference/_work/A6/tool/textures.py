"""Converts the shipped DDS textures to PNG and copies the app logos. Agent A6; writes under DesignReference/_work/ only.

Each DDS is decoded here from its header (DDS_PIXELFORMAT masks), and Pillow's own DDS reader is used as a cross-check.
Outputs, per texture <name>:
  textures/<name>.png          the texels as stored, RGBA, straight
  textures/<name>_on_dark.png  the texels alpha-blended over a dark checker, so that a white-with-alpha texture shows its
                               shape; enlarged with nearest-neighbour to at least 512 px on its long side. (The game's one
                               loaded texture, Particle.dds, has alpha 255 everywhere and is added ONE + ONE with its alpha
                               ignored: GlowPipeline.cpp:72-76, GlowSpritePS.hlsl; so this shows it as it adds.)
  textures/<name>_alpha.png    the alpha channel as gray, same enlargement
"""

import hashlib
import json
import shutil
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
ASSETS = ROOT / "OutpostCommander" / "Assets"
WORK = ROOT / "DesignReference" / "_work"
OUT_TEXTURES = WORK / "assets" / "textures"
OUT_LOGOS = WORK / "assets" / "logos"

FOURCC_NAMES = {b"DXT1": "BC1 (DXT1)", b"DXT3": "BC2 (DXT3)", b"DXT5": "BC3 (DXT5)", b"DX10": "DX10 extended header"}


def mask_shift(mask):
  if mask == 0:
    return None
  shift = 0
  while not (mask >> shift) & 1:
    shift += 1
  return shift


def decode_dds(path):
  data = path.read_bytes()
  if data[:4] != b"DDS ":
    raise ValueError(f"{path.name}: not a DDS file")
  size, flags, height, width, pitch, depth, mips = struct.unpack_from("<7I", data, 4)
  pf_size, pf_flags, fourcc, bits, rmask, gmask, bmask, amask = struct.unpack_from("<2I4s5I", data, 76)
  caps = struct.unpack_from("<4I", data, 108)
  info = {
    "file": f"OutpostCommander/Assets/Textures/{path.name}",
    "bytes": len(data),
    "sha256": hashlib.sha256(data).hexdigest(),
    "width": width,
    "height": height,
    "mipCount": mips if flags & 0x20000 else 1,
    "headerFlags": hex(flags),
    "pixelFormatFlags": hex(pf_flags),
    "caps": [hex(c) for c in caps],
  }
  if pf_flags & 0x4:  # DDPF_FOURCC
    info["format"] = FOURCC_NAMES.get(fourcc, repr(fourcc))
    raise NotImplementedError(f"{path.name}: compressed format {info['format']} is not decoded here")
  if not (pf_flags & 0x40) or bits != 32:  # DDPF_RGB, 32 bits
    raise NotImplementedError(f"{path.name}: pixel format flags {pf_flags:#x}, {bits} bits")
  names = {0x00FF0000: "R", 0x0000FF00: "G", 0x000000FF: "B", 0xFF000000: "A"}
  byte_order = "".join(names.get(m, "?") for m in sorted([rmask, gmask, bmask, amask], key=lambda m: mask_shift(m) or 0))
  info["format"] = (f"uncompressed 32-bit, masks R {rmask:#010x} G {gmask:#010x} B {bmask:#010x} A {amask:#010x} "
                    f"(bytes {byte_order} in memory: D3DFMT_A8R8G8B8 / DXGI_FORMAT_B8G8R8A8_UNORM)"
                    + (", alpha present" if pf_flags & 0x1 else ""))
  offset = 128
  texels = np.frombuffer(data, dtype="<u4", count=width * height, offset=offset).reshape(height, width)
  channels = []
  for mask in (rmask, gmask, bmask, amask):
    shift = mask_shift(mask)
    channels.append(((texels & mask) >> shift).astype(np.uint8) if shift is not None else np.full((height, width), 255, np.uint8))
  rgba = np.stack(channels, axis=-1)
  info["trailingBytes"] = len(data) - offset - width * height * 4
  return rgba, info


def checker(width, height, cell=16):
  y, x = np.mgrid[0:height, 0:width]
  dark = ((x // cell + y // cell) % 2).astype(np.float32)
  base = np.stack([dark * 10 + 14, dark * 10 + 16, dark * 12 + 22], axis=-1)
  return base


def enlarge(image, minimum=512):
  factor = max(1, minimum // max(image.size))
  return image.resize((image.size[0] * factor, image.size[1] * factor), Image.NEAREST), factor


def main():
  OUT_TEXTURES.mkdir(parents=True, exist_ok=True)
  OUT_LOGOS.mkdir(parents=True, exist_ok=True)
  records, table = [], []
  for path in sorted((ASSETS / "Textures").glob("*.dds")):
    name = path.stem
    rgba, info = decode_dds(path)
    # Pillow's reader, as a cross-check of the decode.
    try:
      with Image.open(path) as pillow:
        pillow_rgba = np.asarray(pillow.convert("RGBA"))
      info["pillowAgrees"] = bool(np.array_equal(pillow_rgba, rgba))
    except Exception as error:  # noqa: BLE001
      info["pillowAgrees"] = f"Pillow could not read it: {error}"
    stored = Image.fromarray(rgba, "RGBA")
    stored.save(OUT_TEXTURES / f"{name}.png", optimize=True)
    alpha = rgba[..., 3:4].astype(np.float32) / 255.0
    background = checker(rgba.shape[1], rgba.shape[0], cell=max(2, rgba.shape[1] // 8))
    on_dark = background * (1.0 - alpha) + rgba[..., :3].astype(np.float32) * alpha
    dark_image, factor = enlarge(Image.fromarray(np.clip(on_dark, 0, 255).astype(np.uint8), "RGB"))
    dark_image.save(OUT_TEXTURES / f"{name}_on_dark.png", optimize=True)
    alpha_image, _ = enlarge(Image.fromarray(rgba[..., 3], "L"))
    alpha_image.save(OUT_TEXTURES / f"{name}_alpha.png", optimize=True)
    rgb = rgba[..., :3].astype(int)
    info.update({
      "rgbMin": rgb.reshape(-1, 3).min(axis=0).tolist(), "rgbMax": rgb.reshape(-1, 3).max(axis=0).tolist(),
      "alphaMin": int(rgba[..., 3].min()), "alphaMax": int(rgba[..., 3].max()),
      "grayscale": bool(np.all(rgb[..., 0] == rgb[..., 1]) and np.all(rgb[..., 1] == rgb[..., 2])),
      "alphaEqualsRed": bool(np.array_equal(rgba[..., 3], rgba[..., 0])),
      "enlargement": factor,
    })
    table.append(info)
    for out, kind, size in ((f"{name}.png", "texture as stored", stored.size), (f"{name}_on_dark.png", "texture alpha-blended over a dark checker, enlarged", dark_image.size),
                            (f"{name}_alpha.png", "texture alpha as gray, enlarged", alpha_image.size)):
      records.append({"file": str((OUT_TEXTURES / out).relative_to(ROOT)), "set": None, "model": name, "view": None,
                      "size": list(size), "kind": kind, "source": info["file"]})
  logos = []
  for path in sorted(ASSETS.glob("*.png")):
    target = OUT_LOGOS / path.name
    shutil.copyfile(path, target)
    with Image.open(path) as image:
      logos.append({"file": f"OutpostCommander/Assets/{path.name}", "size": list(image.size), "mode": image.mode,
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size})
      records.append({"file": str(target.relative_to(ROOT)), "set": None, "model": path.stem, "view": None, "size": list(image.size),
                      "kind": "app logo (copied as is)", "source": f"OutpostCommander/Assets/{path.name}"})
  # A sheet of the logos on the sky's near-black and on white, since each is one light gray in its alpha alone. Each
  # image's extent is outlined and its file named under it.
  from PIL import ImageDraw, ImageFont
  label_font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13)
  logos_by_name = {path.name: path for path in sorted(ASSETS.glob("*.png"))}
  layout = [("SplashScreen.scale-200.png", 10, 10)]
  x = 10
  for name in ("Wide310x150Logo.scale-200.png", "Square150x150Logo.scale-200.png", "Square44x44Logo.scale-200.png",
               "StoreLogo.png", "LockScreenLogo.scale-200.png", "Square44x44Logo.targetsize-24_altform-unplated.png"):
    layout.append((name, x, 640))
    with Image.open(logos_by_name[name]) as image:
      x += max(image.size[0], 230) + 20
  half_height = 640 + 300 + 40
  sheet = Image.new("RGBA", (2200, 2 * half_height), (5, 6, 10, 255))
  sheet.paste(Image.new("RGBA", (2200, half_height), (255, 255, 255, 255)), (0, half_height))
  draw = ImageDraw.Draw(sheet)
  for half, (outline, text) in enumerate((((70, 74, 90, 255), (170, 175, 190, 255)), ((200, 200, 205, 255), (90, 90, 100, 255)))):
    for name, px, py in layout:
      with Image.open(logos_by_name[name]) as image:
        image = image.convert("RGBA")
        top = py + half * half_height
        draw.rectangle((px - 1, top - 1, px + image.size[0], top + image.size[1]), outline=outline)
        sheet.alpha_composite(image, (px, top))
        draw.text((px, top + image.size[1] + 4), name, font=label_font, fill=text)
  sheet.convert("RGB").save(OUT_LOGOS / "_logos_on_dark_and_white.png", optimize=True)
  records.append({"file": str((OUT_LOGOS / "_logos_on_dark_and_white.png").relative_to(ROOT)), "set": None, "model": "logos",
                  "view": None, "size": [2200, 2 * half_height], "kind": "logo sheet (each logo on near-black, then on white)", "source": None})
  (WORK / "A6" / "textures.json").write_text(json.dumps({"textures": table, "logos": logos}, indent=1))
  (WORK / "A6" / "renders_textures.json").write_text(json.dumps(records, indent=1))
  print(json.dumps(table, indent=1))
  print(json.dumps(logos, indent=1))


if __name__ == "__main__":
  sys.exit(main())
