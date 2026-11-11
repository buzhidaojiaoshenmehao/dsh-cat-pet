import { queryBalance } from './balance.js'
import { startPet } from './bridge.js'
import { createConnectionReader } from './connection.js'
import { observeHarness } from './tasks.js'
import { activityPacket, balancePacket, taskPacket } from './wire.js'

export const name = 'cat-pet'
export const inject = ['sessions']

export function apply(ctx, config = {}) {
  if (process.platform !== 'win32' || config.enabled === false) return
  const lifetime = new AbortController()
  const connection = createConnectionReader(ctx)
  let querying = false
  let observer
  const pet = startPet(() => {
    if (querying || lifetime.signal.aborted) return
    querying = true
    pet.send(balancePacket({ status: 'loading' }))
    void queryBalance(ctx, observer?.balanceProvider, connection, lifetime.signal).then(result => {
      querying = false
      if (!lifetime.signal.aborted) pet.send(balancePacket(result))
    })
  }, message => ctx.logger.warn(message))

  observer = observeHarness(ctx, {
    connection,
    onActivity: (state, count) => pet.send(activityPacket(state, count)),
    onTask: task => pet.send(taskPacket(task)),
  })
  ctx.on('deepseek-account/signed-out', () => pet.send(balancePacket({ status: 'idle' })))
  ctx.on('dispose', () => {
    lifetime.abort()
    pet.stop()
  })
}
