import { execFileSync } from 'node:child_process'
import { copyFileSync, cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, renameSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createHash } from 'node:crypto'

const root = fileURLToPath(new URL('../', import.meta.url))
const { name, version } = JSON.parse(readFileSync(join(root, 'package.json'), 'utf8'))
const dist = join(root, 'dist')
mkdirSync(dist, { recursive: true })
for (const arch of ['x64', 'arm64']) {
  if (!existsSync(join(root, 'bin', `win-${arch}`, 'LittlePaw.exe'))) throw new Error(`Build win-${arch} first`)
}
for (const sprite of ['idle', 'curious', 'paw', 'sleep', 'belly', 'crouch', 'walk-1', 'walk-2', 'walk-3', 'walk-4']) {
  if (!existsSync(join(root, 'bin', 'assets', `${sprite}.png`))) throw new Error(`Missing sprite ${sprite}`)
}

const stage = mkdtempSync(join(dist, '.pack-'))
const archive = join(dist, `LittlePaw-DeepSeek-Harness-Windows-${version}.zip`)
try {
  const [{ filename }] = JSON.parse(execFileSync('npm', ['pack', '--json', '--pack-destination', stage], { cwd: root }))
  const tarball = join(stage, filename)
  const folder = join(stage, 'LittlePaw-DeepSeek-Harness')
  mkdirSync(folder)
  copyFileSync(tarball, join(folder, filename))
  copyFileSync(join(root, 'README.md'), join(folder, 'README.md'))
  mkdirSync(join(folder, 'assets'))
  copyFileSync(join(root, 'assets', 'PROMPTS.md'), join(folder, 'assets', 'PROMPTS.md'))
  cpSync(join(root, 'bin'), join(folder, 'bin'), { recursive: true, filter: path => !path.endsWith('.DS_Store') })
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
