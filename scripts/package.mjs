import { execFileSync } from 'node:child_process'
import { copyFileSync, cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, renameSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createHash } from 'node:crypto'

const root = fileURLToPath(new URL('../', import.meta.url))
const { name, version } = JSON.parse(readFileSync(join(root, 'package.json'), 'utf8'))
const dist = join(root, 'dist')
const macExecutable = 'bin/macos/LittlePaw.app/Contents/MacOS/LittlePaw'
mkdirSync(dist, { recursive: true })
for (const arch of ['x64', 'arm64']) {
  if (!existsSync(join(root, 'bin', `win-${arch}`, 'LittlePaw.exe'))) throw new Error(`Build win-${arch} first`)
}
if (!existsSync(join(root, macExecutable))) throw new Error('Build macOS first')
if (!existsSync(join(root, 'bin', 'assets', 'meow.wav'))) throw new Error('Missing meow audio')
const motionFrames = (prefix, count) => Array.from({ length: count }, (_, i) => `${prefix}-${String(i).padStart(2, '0')}`)
const sprites = ['idle', 'curious', 'paw', 'sleep', 'belly', 'crouch', 'icon',
  ...motionFrames('gait', 24), ...motionFrames('stretch', 14)]
for (const sprite of sprites) {
  if (!existsSync(join(root, 'bin', 'assets', `${sprite}.png`))) throw new Error(`Missing sprite ${sprite}`)
}

const stage = mkdtempSync(join(dist, '.pack-'))
const archive = join(dist, `LittlePaw-DeepSeek-Harness-Windows-${version}.zip`)
try {
  const [{ filename, files }] = JSON.parse(execFileSync('npm', ['pack', '--json', '--pack-destination', stage], { cwd: root }))
  if (!(files.find(file => file.path === macExecutable)?.mode & 0o111)) {
    throw new Error('The plugin archive must contain an executable macOS desktop cat')
  }
  const tarball = join(stage, filename)
  const folder = join(stage, 'LittlePaw-DeepSeek-Harness')
  mkdirSync(folder)
  copyFileSync(tarball, join(folder, filename))
  copyFileSync(join(root, 'README.md'), join(folder, 'README.md'))
  mkdirSync(join(folder, 'assets'))
  copyFileSync(join(root, 'assets', 'PROMPTS.md'), join(folder, 'assets', 'PROMPTS.md'))
  copyFileSync(join(root, 'assets', 'AUDIO.md'), join(folder, 'assets', 'AUDIO.md'))
  for (const directory of ['assets', 'win-x64', 'win-arm64']) {
    cpSync(join(root, 'bin', directory), join(folder, 'bin', directory),
      { recursive: true, filter: path => !path.endsWith('.DS_Store') })
  }
  writeFileSync(join(folder, 'preview.cmd'), [
    '@echo off',
    'setlocal',
    'set "paw_arch=win-x64"',
    'if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "paw_arch=win-arm64"',
    'if /i "%PROCESSOR_ARCHITEW6432%"=="ARM64" set "paw_arch=win-arm64"',
    String.raw`start "" "%~dp0bin\%paw_arch%\LittlePaw.exe" --demo`,
    '',
  ].join('\r\n'))
  const hash = file => createHash('sha256').update(readFileSync(file)).digest('hex')
  writeFileSync(join(folder, 'SHA256SUMS.txt'), `${hash(tarball)}  ${filename}\n`)
  const stagedArchive = join(stage, 'release.zip')
  execFileSync('zip', ['-X', '-q', '-r', stagedArchive, 'LittlePaw-DeepSeek-Harness'], { cwd: stage })
  execFileSync('unzip', ['-tq', stagedArchive], { stdio: 'inherit' })
  renameSync(tarball, join(dist, filename))
  renameSync(stagedArchive, archive)
  console.log(`${name} ${version}\n${join(dist, filename)}\n${archive}`)
} finally {
  rmSync(stage, { recursive: true, force: true })
}
