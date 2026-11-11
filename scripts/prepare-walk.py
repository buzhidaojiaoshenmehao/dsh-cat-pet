from pathlib import Path

import cv2
import numpy as np


ASSETS = Path(__file__).resolve().parents[1] / "bin" / "assets"


def premultiply(image):
    rgba = image.astype(np.float32) / 255
    rgba[:, :, :3] *= rgba[:, :, 3:]
    return rgba


def grayscale(rgba):
    # A neutral backdrop makes both the coat texture and the silhouette visible to the flow.
    color = rgba[:, :, :3] + (1 - rgba[:, :, 3:]) * .42
    return cv2.cvtColor((color * 255).astype(np.uint8), cv2.COLOR_BGR2GRAY)


def warp(image, flow, portion, grid):
    coordinates = grid - portion * flow
    for _ in range(2):
        sampled_flow = cv2.remap(flow, coordinates, None, cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE)
        coordinates = grid - portion * sampled_flow
    return cv2.remap(image, coordinates, None, cv2.INTER_LINEAR)


def save_frame(path, premultiplied, silhouette):
    alpha = premultiplied[:, :, 3:]
    np.divide(premultiplied[:, :, :3], alpha, out=premultiplied[:, :, :3], where=alpha > 0)
    # Keep one warped silhouette when a paw appears or disappears behind another.
    premultiplied[:, :, 3:] = silhouette[:, :, 3:]
    pixels = np.rint(np.clip(premultiplied, 0, 1) * 255).astype(np.uint8)
    if not cv2.imwrite(str(path), pixels, [cv2.IMWRITE_PNG_COMPRESSION, 8]):
        raise RuntimeError(f"Could not write {path.name}")


frames = [premultiply(cv2.imread(str(ASSETS / f"walk-{number}.png"), cv2.IMREAD_UNCHANGED))
          for number in range(1, 5)]
height, width = frames[0].shape[:2]
grid = np.stack(np.meshgrid(np.arange(width), np.arange(height)), axis=-1).astype(np.float32)
flow = cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM)
for index, first in enumerate(frames):
    second = frames[(index + 1) % len(frames)]
    gray_first, gray_second = grayscale(first), grayscale(second)
    forward = flow.calc(gray_first, gray_second, None)
    backward = flow.calc(gray_second, gray_first, None)
    for letter, portion in (("a", 1 / 3), ("b", 2 / 3)):
        a = warp(first, forward, portion, grid)
        b = warp(second, backward, 1 - portion, grid)
        silhouette = a if portion < .5 else b
        save_frame(ASSETS / f"walk-{index + 1}-{letter}.png", a * (1 - portion) + b * portion, silhouette)
