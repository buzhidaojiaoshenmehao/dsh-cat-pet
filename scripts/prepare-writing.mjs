import { execFileSync } from 'node:child_process'
import { mkdirSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = fileURLToPath(new URL('../', import.meta.url))
const destination = join(root, 'bin', 'assets')
const width = 384, height = 404, frames = 20
const crop = { x: 92, y: 48, width: 1112, height: 1168 }
const source = execFileSync('ffmpeg', [
  '-v', 'error', '-i', join(root, 'assets', 'atlases', 'writing.png'), '-frames:v', '1',
  '-vf', `crop=${crop.width}:${crop.height}:${crop.x}:${crop.y},scale=${width}:${height}:flags=lanczos`,
  '-f', 'rawvideo', '-pix_fmt', 'rgba', '-threads', '1', 'pipe:1',
])
if (source.length !== width * height * 4 || source[3] !== 0) throw new Error('The writing sprite needs transparent edges')
mkdirSync(destination, { recursive: true })

function smooth(value) {
  const t = Math.max(0, Math.min(1, value))
  return t * t * (3 - 2 * t)
}

// Keep the head, supporting paw and page edges still. Move the writing
// paw and the pencil together, with slightly more travel at the pencil tip.
const motion = new Float32Array(width * height)
for (let y = 0; y < height; y++) {
  const sy = crop.y + y * crop.height / height
  if (sy < 638 || sy >= 1163) continue
  for (let x = 0; x < width; x++) {
    const sx = crop.x + x * crop.width / width
    const pencilX = 222 + .40 * (sy - 690)
    const pencil = 1 - smooth((Math.abs(sx - pencilX - 17) - 60) / 69)
    const forepaw = smooth((sx - 115) / 83) * (1 - smooth((sx - 386) / 82))
      * smooth((sy - 781) / 58) * (1 - smooth((sy - 1006) / 50))
    const envelope = smooth((sy - 638) / 48) * (1 - smooth((sy - 1105) / 58))
    motion[y * width + x] = Math.max(pencil, forepaw) * envelope * (.63 + .37 * smooth((sy - 800) / 294))
  }
}

function sample(frame, offset, x, y) {
  x = Math.max(0, Math.min(width - 1.001, x))
  y = Math.max(0, Math.min(height - 1.001, y))
  const left = Math.floor(x), top = Math.floor(y)
  const u = x - left, v = y - top
  const a = (top * width + left) * 4, b = a + width * 4
  const topLeft = (1 - u) * (1 - v) * source[a + 3]
  const topRight = u * (1 - v) * source[a + 7]
  const bottomLeft = (1 - u) * v * source[b + 3]
  const bottomRight = u * v * source[b + 7]
  const alpha = topLeft + topRight + bottomLeft + bottomRight
  frame[offset + 3] = Math.round(alpha)
  if (alpha < .5) return
  for (let c = 0; c < 3; c++) {
    frame[offset + c] = Math.round((topLeft * source[a + c] + topRight * source[a + c + 4]
      + bottomLeft * source[b + c] + bottomRight * source[b + c + 4]) / alpha)
  }
}

for (let i = 0; i < frames; i++) {
  const phase = i / frames
  const writing = Math.min(1, phase / .75)
  const resetting = Math.max(0, (phase - .75) / .25)
  const travel = smooth(writing) - smooth(resetting)
  const down = Math.sin(Math.PI * writing)
  const returnLift = Math.sin(Math.PI * resetting)
  const dx = (63 * travel - 14 + 8 * down * Math.sin(writing * Math.PI * 6)) * width / crop.width
  const dy = (12 * down * Math.sin(writing * Math.PI * 6) - 33 * returnLift ** 2) * height / crop.height
  const frame = Buffer.from(source)
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const pixel = y * width + x
      const weight = motion[pixel]
      if (weight > .001) sample(frame, pixel * 4, x - weight * dx, y - weight * dy)
    }
  }
  const filename = `write-${String(i).padStart(2, '0')}.png`
  execFileSync('ffmpeg', [
    '-v', 'error', '-y', '-f', 'rawvideo', '-pixel_format', 'rgba', '-video_size', `${width}x${height}`,
    '-i', 'pipe:0', '-frames:v', '1', '-threads', '1', join(destination, filename),
  ], { input: frame })
}
console.log(`Writing animation: ${frames} frames`)
