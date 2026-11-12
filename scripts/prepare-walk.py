from pathlib import Path

import cv2
import numpy as np


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "bin" / "assets"
REFERENCE = ROOT / ".build" / "walk-source" / "walk-1.png"
GAIT_FRAMES = 24
STRETCH_FRAMES = 14


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def premultiply(image):
    image = image.astype(np.float32) / 255
    image[:, :, :3] *= image[:, :, 3:]
    return image


def cut(image, vertices, feather=1.2):
    mask = np.zeros(image.shape[:2], dtype=np.uint8)
    cv2.fillPoly(mask, [np.array(vertices, dtype=np.int32)], 255, lineType=cv2.LINE_AA)
    mask = cv2.GaussianBlur(mask, (0, 0), feather).astype(np.float32) / 255
    return image * mask[:, :, None]


def over(back, front):
    return front + back * (1 - front[:, :, 3:])


def save(path, image):
    # Resize in premultiplied color to keep the silhouette clear on a dark desktop.
    image = cv2.resize(image, (448, 235), interpolation=cv2.INTER_AREA)
    alpha = image[:, :, 3:]
    np.divide(image[:, :, :3], alpha, out=image[:, :, :3], where=alpha > 0)
    pixels = np.rint(np.clip(image, 0, 1) * 255).astype(np.uint8)
    if not cv2.imwrite(str(path), pixels, [cv2.IMWRITE_PNG_COMPRESSION, 8]):
        raise RuntimeError(f"Could not write {path.name}")


def body_offset(x, stretch):
    front = 1 - smooth((x - 78) / 285)
    tail = smooth((x - 442) / 95)
    return -29 * front * stretch, (52 * front - 15 * tail) * stretch


def warp_body(image, stretch, x, y):
    source_x = x.copy()
    for _ in range(3):
        dx, _ = body_offset(source_x, stretch)
        source_x = x - dx
    _, dy = body_offset(source_x, stretch)
    return cv2.remap(image, source_x.astype(np.float32), (y - dy).astype(np.float32), cv2.INTER_LINEAR)


def foot_position(phase, offset, center, floor, height):
    progress = (phase - offset) % 1
    contact = .63
    stride = 82
    if progress < contact:
        return center + stride * (progress / contact - .5), floor
    swing = (progress - contact) / (1 - contact)
    return center + stride * (.5 - smooth(swing)), floor - height * np.sin(np.pi * swing)


def warp_leg(image, root, paw, target, stretch, x, y):
    rx, ry = root
    px, py = paw
    tx, ty = target
    dx, dy = body_offset(rx, stretch)
    # Keep the shoulder or hip attached as the ankle moves. The paw stays horizontal.
    source_y = ry + (y - (ry + dy)) * (py - ry) / (ty - (ry + dy))
    source_y = np.where(y < ry + dy, y - dy, source_y)
    fraction = np.clip((source_y - ry) / (py - ry), 0, 1)
    source_x = x - dx - (tx - px - dx) * fraction
    return cv2.remap(image, source_x.astype(np.float32), source_y.astype(np.float32), cv2.INTER_LINEAR)


source = cv2.imread(str(REFERENCE), cv2.IMREAD_UNCHANGED)
if source is None or source.shape != (283, 540, 4):
    raise RuntimeError("Run scripts/prepare-sprites.mjs to prepare the 540×283 walk reference")
cat = premultiply(source)
y, x = np.indices(cat.shape[:2], dtype=np.float32)
body = cut(cat, [
    (0, 0), (539, 0), (539, 229), (479, 227), (446, 207), (430, 191), (410, 207),
    (377, 207), (350, 213), (316, 224), (281, 225), (242, 224), (215, 216),
    (181, 207), (153, 210), (127, 203), (101, 203), (79, 179), (0, 169),
])

# Far hind, far front, near hind, near front. The four footfalls follow a lateral sequence.
# Each mask overlaps its attachment beneath the coat, with paws cut from the same cat.
legs = [
    ([(292, 178), (348, 180), (353, 219), (340, 240), (318, 273), (281, 281), (251, 276),
      (253, 255), (277, 239), (299, 212)], (323, 197), (274, 263), .50, 307, 263, 24, 307),
    ([(74, 152), (122, 169), (133, 194), (100, 210), (76, 222), (28, 256), (0, 259),
      (0, 224), (34, 199), (61, 179)], (98, 175), (18, 240), .75, 111, 260, 24, 17),
    ([(363, 154), (409, 148), (433, 162), (442, 194), (471, 224), (485, 254), (480, 282),
      (428, 282), (428, 253), (409, 230), (381, 214), (355, 195)], (405, 180), (459, 264),
     .00, 433, 269, 28, 433),
    ([(150, 169), (206, 169), (221, 189), (216, 219), (196, 275), (174, 282), (145, 279),
      (146, 255), (157, 229), (162, 204)], (185, 176), (171, 268), .25, 171, 270, 29, 65),
]
textures = [cut(cat, leg[0]) for leg in legs]
for texture in textures[:2]:
    texture[:, :, :3] *= .92


def render(phase=None, stretch=0):
    limbs = []
    for (_, root, paw, offset, center, floor, lift, extended), texture in zip(legs, textures):
        if phase is not None:
            target = foot_position(phase, offset, center, floor, lift)
        else:
            target = (center + (extended - center) * stretch, floor)
        limbs.append(warp_leg(texture, root, paw, target, stretch, x, y))
    result = over(limbs[0], limbs[1])
    result = over(result, warp_body(body, stretch, x, y))
    return over(over(result, limbs[2]), limbs[3])


for i in range(GAIT_FRAMES):
    save(ASSETS / f"gait-{i:02d}.png", render(phase=i / GAIT_FRAMES))
for i in range(STRETCH_FRAMES):
    save(ASSETS / f"stretch-{i:02d}.png", render(stretch=i / (STRETCH_FRAMES - 1)))
