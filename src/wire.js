import { accountProvider, apiProvider } from './connection.js'

const cell = value => value === null || value === undefined ? '-' : String(value)
const packet = fields => fields.map(cell).join('\t') + '\n'

export const activityPacket = (state, count) => packet(['activity', state, count])

export function taskPacket(task) {
  const mode = task.route === accountProvider ? 'account' : task.route === apiProvider ? 'api' : undefined
  return packet(['task', task.outcome, task.child ? 'child' : 'main', task.total, task.input, task.output,
    task.cached, task.CNY, task.USD, task.coverage, Number(task.incompleteTokens), task.at, task.reports, mode])
}

export function balancePacket(result) {
  if (result.status !== 'ready') return packet(['balance', result.status])
  return packet(['balance', 'ready', result.mode,
    result.available === undefined ? undefined : Number(result.available),
    ...result.wallets.flatMap(wallet => [wallet.currency, wallet.total, wallet.granted, wallet.topped])])
}
