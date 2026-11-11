import { spawn } from 'node:child_process'
import { existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { createInterface } from 'node:readline'

const windowsEnvironment = new Set(['systemroot', 'windir', 'temp', 'tmp', 'localappdata', 'userprofile'])
const macosEnvironment = new Set(['home', 'tmpdir', 'lang', 'lc_all', 'lc_ctype'])

export function startPet(onRequest, warn) {
  const arch = process.arch === 'arm64' ? 'arm64' : process.arch === 'x64' ? 'x64' : undefined
  const macos = process.platform === 'darwin'
  const os = macos ? 'macOS' : 'Windows'
  const executable = macos ? 'macos/LittlePaw.app/Contents/MacOS/LittlePaw' : `win-${arch}/LittlePaw.exe`
  const path = arch && fileURLToPath(new URL(`../bin/${executable}`, import.meta.url))
  if (!path || !existsSync(path)) {
    warn(`${os} desktop cat binary is missing or this CPU is unsupported.`)
    return { send() {}, stop() {} }
  }

  const allowed = macos ? macosEnvironment : windowsEnvironment
  const env = Object.fromEntries(Object.entries(process.env).filter(([key]) => allowed.has(key.toLowerCase())))
  let child
  let restart
  let stopped = false
  let failures = 0
  let activity = 'activity\tidle\t0\n'
  let task

  function send(line) {
    if (line.startsWith('activity\t')) activity = line
    if (line.startsWith('task\t')) task = line
    if (child?.stdin.writable && child.stdin.writableLength < 65536) child.stdin.write(line)
  }

  function launch() {
    if (stopped) return
    const started = Date.now()
    const processHandle = spawn(path, ['--bridge'], { stdio: ['pipe', 'pipe', 'ignore'], env, windowsHide: true })
    child = processHandle
    processHandle.stdin.on('error', () => {})
    processHandle.on('error', () => warn(`${os} desktop cat could not start.`))
    const lines = createInterface({ input: processHandle.stdout, crlfDelay: Infinity })
    lines.on('line', line => {
      if (stopped) return
      if (line === 'exit') stopped = true
      else if (line === 'balance') onRequest()
    })
    processHandle.once('close', () => {
      lines.close()
      child = undefined
      if (stopped) return
      failures = Date.now() - started > 60000 ? 0 : failures + 1
      if (failures <= 2) {
        restart = setTimeout(launch, 1500 * Math.max(failures, 1))
        restart.unref()
      }
      else warn(`${os} desktop cat exited repeatedly. Restart Harness to try again.`)
    })
    // A short-lived Harness command can finish while the companion is still visible.
    // The companion exits on EOF when the host closes its end of the pipe.
    processHandle.unref()
    processHandle.stdin.unref?.()
    processHandle.stdout.unref?.()
    send('hello\t1\n')
    send(activity)
    if (task) send(task.replace(/^task\t/, 'recent\t'))
  }

  launch()
  return {
    send,
    stop() {
      stopped = true
      clearTimeout(restart)
      if (!child) return
      const handle = child
      handle.stdin.end('quit\n')
      const deadline = setTimeout(() => handle.kill(), 2000)
      deadline.unref()
      handle.once('close', () => clearTimeout(deadline))
    },
  }
}
