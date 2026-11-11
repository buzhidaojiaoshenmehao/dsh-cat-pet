import { execFileSync } from 'node:child_process'
import { cpSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { writePricingCalendar } from './pricing-calendar.mjs'

if (process.platform !== 'darwin') throw new Error('The macOS build requires macOS and Xcode or Command Line Tools.')

const root = fileURLToPath(new URL('../', import.meta.url))
const { version } = JSON.parse(readFileSync(join(root, 'package.json'), 'utf8'))
const bundle = join(root, 'bin', 'macos', 'LittlePaw.app')
// Signing a bundle on a synced volume can fail when Finder adds extended attributes.
const stage = mkdtempSync(join(tmpdir(), 'little-paw-macos-'))
const stagedBundle = join(stage, 'LittlePaw.app')
const contents = join(stagedBundle, 'Contents')
const executable = join(contents, 'MacOS', 'LittlePaw')
const generated = join(root, '.build', 'macos')
const info = readFileSync(join(root, 'macos', 'Info.plist'), 'utf8').replaceAll('@VERSION@', version)

try {
  writePricingCalendar(generated)
  mkdirSync(join(contents, 'MacOS'), { recursive: true })
  writeFileSync(join(contents, 'Info.plist'), info)
  execFileSync('xcrun', ['--sdk', 'macosx', 'clang++', '-arch', 'arm64', '-arch', 'x86_64',
    '-mmacosx-version-min=13.0', '-std=c++20', '-fobjc-arc', '-O2', '-Wall', '-Wextra',
    '-I', 'native', '-I', generated, 'macos/main.mm', 'macos/panel.mm',
    '-framework', 'AppKit', '-framework', 'CoreGraphics', '-o', executable], { cwd: root, stdio: 'inherit' })
  execFileSync('codesign', ['--force', '--sign', '-', '--timestamp=none', stagedBundle], { stdio: 'inherit' })
  execFileSync('codesign', ['--verify', '--strict', stagedBundle], { stdio: 'inherit' })
  const architectures = execFileSync('lipo', ['-archs', executable], { encoding: 'utf8' }).trim().split(/\s+/)
  if (!architectures.includes('arm64') || !architectures.includes('x86_64')) {
    throw new Error('The macOS executable must include Apple silicon and Intel architectures')
  }
  rmSync(bundle, { recursive: true, force: true })
  cpSync(stagedBundle, bundle, { recursive: true })
  console.log('Built macOS universal desktop cat (arm64 and x86_64, macOS 13+)')
} finally {
  rmSync(stage, { recursive: true, force: true })
}
