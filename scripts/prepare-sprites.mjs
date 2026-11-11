import { execFileSync } from 'node:child_process'
import { mkdirSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { join } from 'node:path'

const root = fileURLToPath(new URL('../', import.meta.url))
const output = join(root, 'bin', 'assets')
mkdirSync(output, { recursive: true })

// Slice the generated atlases. Keep their alpha and align all walk frames to one common crop.
function slice(file, columns, rows, names, align) {
  const input = join(root, 'assets', 'atlases', file)
  const probe = execFileSync('ffprobe', ['-v', 'error', '-show_entries', 'stream=width,height,pix_fmt', '-of', 'json', input])
  const { width, height, pix_fmt } = JSON.parse(probe).streams[0]
  if (pix_fmt !== 'rgba' || width % columns || height % rows) throw new Error(`Unexpected atlas format for ${file}`)
  const pixels = execFileSync('ffmpeg', ['-v', 'error', '-i', input, '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgba', 'pipe:1'],
    { maxBuffer: width * height * 4 + 4096 })
  const cellW = width / columns
  const cellH = height / rows
  const boxes = names.map((name, index) => {
    const x0 = (index % columns) * cellW
    const y0 = Math.floor(index / columns) * cellH
    const box = { x0, y0, left: cellW, top: cellH, right: -1, bottom: -1 }
    let transparent = 0
    for (let y = 0; y < cellH; y++) {
      for (let x = 0; x < cellW; x++) {
        const alpha = pixels[((y0 + y) * width + x0 + x) * 4 + 3]
        if (alpha === 0) transparent++
        if (alpha < 80) continue
        box.left = Math.min(box.left, x)
        box.top = Math.min(box.top, y)
        box.right = Math.max(box.right, x)
        box.bottom = Math.max(box.bottom, y)
      }
    }
    if (box.right < box.left || transparent < cellW * cellH * 0.1) throw new Error(`Bad alpha in ${name}`)
    return box
  })
  const union = {
    left: Math.min(...boxes.map(box => box.left)), top: Math.min(...boxes.map(box => box.top)),
    right: Math.max(...boxes.map(box => box.right)), bottom: Math.max(...boxes.map(box => box.bottom)),
  }
  for (const [index, box] of boxes.entries()) {
    const bounds = align ? union : box
    const left = Math.max(0, bounds.left - 9)
    const top = Math.max(0, bounds.top - 9)
    const w = Math.min(cellW, bounds.right + 10) - left
    const h = Math.min(cellH, bounds.bottom + 10) - top
    const shrink = Math.min(1, 540 / Math.max(w, h))
    const filter = `format=rgba,crop=${w}:${h}:${box.x0 + left}:${box.y0 + top},scale=${Math.round(w * shrink)}:${Math.round(h * shrink)}:flags=lanczos,format=rgba`
    execFileSync('ffmpeg', ['-v', 'error', '-y', '-i', input, '-frames:v', '1', '-vf', filter,
      '-threads', '1', join(output, `${names[index]}.png`)])
  }
  console.log(`${file}: ${names.join(', ')}`)
}

slice('poses.png', 3, 2, ['idle', 'curious', 'paw', 'sleep', 'belly', 'crouch'], false)
slice('walk.png', 2, 2, ['walk-1', 'walk-2', 'walk-3', 'walk-4'], true)

execFileSync('ffmpeg', ['-v', 'error', '-y', '-i', join(output, 'paw.png'), '-frames:v', '1',
  '-vf', 'scale=112:112:force_original_aspect_ratio=decrease:flags=lanczos,format=rgba,pad=128:128:(ow-iw)/2:(oh-ih)/2:color=black@0',
  '-threads', '1', join(output, 'icon.png')])
