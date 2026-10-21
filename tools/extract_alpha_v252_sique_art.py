"""Extract the three supplied Sique illustrations without repainting their pixels.

Requires Pillow and NumPy. Originals remain unchanged in source/sique/.
"""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "assets/art/alpha-v252-20261021"


def extract(card):
    source = ART / "source/sique" / (card + ".jpg")
    image = Image.open(source).convert("RGB")
    rgb = np.asarray(image).astype("float32")
    r, g, b = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    if card == "sique-roast":
        # Exclude the smooth brown backdrop, preserving the vivid flame palette.
        mask = (r > 170) & ((g < r * .46) | (g > 128)) & (b < g * .85)
        mask[1160:, :] = False
        # The steak's near-black outline is intentional foreground.
        body = Image.new("L", image.size)
        ImageDraw.Draw(body).polygon([(264,758),(283,724),(352,686),(416,659),(483,637),
            (525,618),(591,620),(616,632),(664,614),(671,603),(713,606),(737,618),
            (778,629),(810,644),(854,661),(888,685),(933,709),(970,739),(1008,775),
            (1011,840),(1013,889),(1003,921),(967,946),(924,974),(866,999),
            (813,1029),(763,1049),(716,1066),(667,1082),(592,1082),(533,1070),
            (480,1051),(430,1037),(389,1019),(352,1000),(324,976),(296,953),
            (275,922),(262,887)], fill=255)
        mask |= np.asarray(body) > 0
    elif card == "sique-blowfire":
        mask = (r > 45) & (r > g * .95) & (r > b * 1.8)
        mask[1160:, 1130:] = False
    else:
        # Neutral grey pot and cream steam are foreground, as are warm flames.
        mask = ((r > 75) & ((r > g * 1.2) | ((r > 125) & (g > 100))))
        body = Image.new("L", image.size)
        ImageDraw.Draw(body).polygon([(319,556),(346,504),(389,458),(450,426),
            (514,408),(578,402),(610,391),(677,391),(705,408),(762,422),(814,437),
            (855,458),(896,489),(923,527),(949,564),(971,618),(973,682),
            (970,741),(971,798),(967,857),(945,906),(904,945),(862,974),
            (813,997),(762,1014),(703,1030),(633,1037),(572,1030),(517,1012),
            (463,985),(416,954),(375,914),(342,867),(324,815),(312,757),
            (309,690),(313,624)], fill=255)
        mask |= np.asarray(body) > 0
        mask[1180:, :] = False
    mask[:, :70] = False
    mask[:, 1170:] = False
    matte = Image.fromarray(mask.astype("uint8") * 255).filter(ImageFilter.MedianFilter(3))
    # Retain crisp pixel edges; one-pixel key cleanup avoids JPEG background fringes.
    output = image.convert("RGBA")
    output.putalpha(matte)
    destination = ART / "processed" / (card + ".png")
    destination.parent.mkdir(exist_ok=True)
    output.save(destination)
    return output


def main():
    images = [extract(card) for card in ("sique-roast", "sique-blowfire", "sique-backfire")]
    preview = Image.new("RGB", (1200, 800), "white")
    for n, im in enumerate(images):
        for row, color in enumerate(("#ffffff", "#17242f")):
            tile = Image.new("RGBA", im.size, color)
            tile.alpha_composite(im)
            preview.paste(tile.convert("RGB").resize((400, 400)), (n * 400, row * 400))
    path = ROOT / "output/alpha-v2.5.2/sique-transparent-preview.jpg"
    path.parent.mkdir(parents=True, exist_ok=True)
    preview.save(path, quality=95)
    print(path)


if __name__ == "__main__":
    main()
