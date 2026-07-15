from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
CANVAS = 1024


def rounded_bar(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int]) -> None:
    height = box[3] - box[1]
    draw.rounded_rectangle(box, radius=height // 2, fill=(255, 255, 255, 255))


def main() -> None:
    ASSETS.mkdir(exist_ok=True)
    image = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    draw.rounded_rectangle((64, 64, 960, 960), radius=224, fill=(10, 89, 247, 255))
    rounded_bar(draw, (236, 250, 326, 774))
    rounded_bar(draw, (432, 356, 782, 456))
    rounded_bar(draw, (432, 568, 782, 668))

    png_path = ASSETS / "text-calculator-icon.png"
    ico_path = ASSETS / "text-calculator.ico"
    image.save(png_path, optimize=True)
    image.save(
        ico_path,
        format="ICO",
        sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48),
               (64, 64), (128, 128), (256, 256)],
    )


if __name__ == "__main__":
    main()
