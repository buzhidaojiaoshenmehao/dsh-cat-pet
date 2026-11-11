import { execFileSync } from 'node:child_process'
import { mkdirSync, readFileSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { writePricingCalendar } from './pricing-calendar.mjs'

const root = fileURLToPath(new URL('../', import.meta.url))
const compiler = process.env.PAW_ZIG || 'zig'
const architectures = process.argv.slice(2)
if (!architectures.length) architectures.push('x64', 'arm64')
const targets = { x64: ['x86_64-windows-gnu', 0x8664], arm64: ['aarch64-windows-gnu', 0xaa64] }
const generated = join(root, '.build')
writePricingCalendar(generated)

for (const architecture of architectures) {
  if (!Object.hasOwn(targets, architecture)) throw new Error(`Unsupported Windows architecture: ${architecture}`)
  const [target, machine] = targets[architecture]
  const directory = join(root, 'bin', `win-${architecture}`)
  const output = join(directory, 'LittlePaw.exe')
  mkdirSync(directory, { recursive: true })
  execFileSync(compiler, ['c++', '-target', target, '-std=c++20', '-O2', '-s', '-municode',
    '-Wl,--subsystem,windows', '-D_WIN32_WINNT=0x0a00', '-Wall', '-Wextra', '-Wno-missing-field-initializers',
    '-I', generated, '-I', 'native', 'windows/main.cpp', '-lgdiplus', '-lgdi32', '-luser32', '-lshell32',
    '-lcomctl32', '-lwinmm', '-o', output], { cwd: root, stdio: 'inherit' })
  const pe = readFileSync(output)
  const header = pe.readUInt32LE(0x3c)
  if (pe.toString('ascii', header, header + 4) !== 'PE\0\0' || pe.readUInt16LE(header + 4) !== machine
      || pe.readUInt16LE(header + 24 + 68) !== 2) throw new Error(`Wrong architecture or GUI subsystem: ${output}`)
  console.log(`Built Windows ${architecture} GUI executable (${pe.length} bytes)`)
}
