import { strict as assert } from 'node:assert'
import test from 'node:test'
import { EventEmitter } from 'node:events'
import { formatCost, normalizeUsage, priceAttempt, pricePeriod, tariffTransitions } from '../src/pricing.js'
import { observeHarness } from '../src/tasks.js'
import { createConnectionReader } from '../src/connection.js'
import { queryBalance } from '../src/balance.js'
import { balancePacket, taskPacket } from '../src/wire.js'

const provider = 'deepseek-official'
const model = 'deepseek-flash'
const peak = Date.parse('2026-11-12T09:15:00+08:00')
const sample = { inputTokens: 1000, cacheReadTokens: 3200, cacheWriteTokens: 0, outputTokens: 480,
  totalTokens: 4680, reasoningTokens: 70 }
const connection = route => ({ official: route === provider, apiKeyEnv: 'DEEPSEEK_API_KEY' })

test('rates use request time, Beijing weekdays, both peak windows and holidays', () => {
  assert.equal(pricePeriod(peak), 'peak')
  assert.equal(pricePeriod(Date.parse('2026-11-12T12:00:00+08:00')), 'off-peak')
  assert.equal(pricePeriod(Date.parse('2026-11-12T14:00:00+08:00')), 'peak')
  assert.equal(pricePeriod(Date.parse('2026-11-12T18:00:00+08:00')), 'off-peak')
  assert.equal(pricePeriod(Date.parse('2027-10-04T09:30:00+08:00')), 'off-peak')
  assert.equal(pricePeriod(Date.parse('2027-10-09T09:30:00+08:00')), 'off-peak')
  const usage = normalizeUsage(sample)
  assert.deepEqual(usage, { total: 4680, input: 4200, output: 480, cached: 3200, exact: true })
  const cost = priceAttempt('deepseek-v4-flash', usage, peak)
  assert.equal(formatCost(cost.CNY), '0.005968')
  assert.equal(formatCost(cost.USD), '0.000895')
  assert.equal(priceAttempt('vendor-pro', usage, peak), undefined)
  assert.equal(formatCost(3n), '<0.000001')
  assert.equal(normalizeUsage({ ...sample, totalTokens: 4700 }), undefined)
  assert.equal(normalizeUsage({ inputTokens: 10, outputTokens: 5 }).exact, false)
})

test('countdown calendar follows each tariff boundary and skips off-peak days', () => {
  const transitions = tariffTransitions()
  for (const [index, boundary] of transitions.entries()) {
    assert.equal(pricePeriod(boundary.at), boundary.period)
    assert.equal(pricePeriod(boundary.at - 1), transitions[index - 1]?.period)
    if (index > 0) assert.ok(boundary.at > transitions[index - 1].at)
  }
  const nextAfter = text => transitions.find(({ at }) => at > Date.parse(text))
  assert.deepEqual(nextAfter('2026-11-12T12:00:00+08:00'), {
    at: Date.parse('2026-11-12T14:00:00+08:00'), period: 'peak',
  })
  assert.deepEqual(nextAfter('2026-11-13T18:00:00+08:00'), {
    at: Date.parse('2026-11-16T09:00:00+08:00'), period: 'peak',
  })
  assert.deepEqual(nextAfter('2027-09-30T18:00:00+08:00'), {
    at: Date.parse('2027-10-08T09:00:00+08:00'), period: 'peak',
  })
  assert.equal(nextAfter('2028-01-01T00:00:00+08:00'), undefined)
})

test('observes simultaneous turns, tools, retries and a local child without counting a stream twice', () => {
  const ctx = new EventEmitter()
  const activities = []
  const tasks = []
  observeHarness(ctx, { connection, onActivity: (...args) => activities.push(args), onTask: task => tasks.push(task), clock: () => peak })
  const session = (id, parentSession) => ({ id, header: parentSession ? { parentSession, origin: 'subagent' } : {},
    requestHeader: () => ({ config: { provider, model } }) })
  const main = session('main')
  const child = session('child', 'main')
  const other = session('other')
  let seq = 0
  const emit = (subject, type, data) => {
    const event = { type, data: { turn: 1, ...data }, seq: seq++, time: peak }
    ctx.emit('session/event', subject, event)
    return event
  }
  emit(main, 'turn/start')
  emit(other, 'turn/start')
  emit(main, 'step/start')
  ctx.emit('agent/assistant-stream', { agent: { session: main }, frame: { type: 'start', turn: 1 } })
  emit(main, 'assistant/attempt', { stream: [{ type: 'chunk', chunk: { type: 'usage', usage: sample } }] })
  emit(main, 'llm/retry-started')
  const settlement = emit(main, 'assistant/message', { message: { source: { provider, model } }, usage: sample,
    stream: [{ type: 'chunk', chunk: { type: 'usage', usage: sample } }] })
  ctx.emit('session/event', main, settlement)
  emit(main, 'tool/call', { callId: 'call' })
  assert.deepEqual(activities.at(-1), ['working', 2])
  emit(child, 'turn/start')
  emit(child, 'assistant/message', { message: { source: { provider, model } }, usage: sample })
  emit(child, 'turn/end', { reason: { kind: 'completed' } })
  assert.equal(tasks.length, 0)
  emit(main, 'tool/result', { message: { toolCallId: 'call' } })
  emit(main, 'turn/end', { reason: { kind: 'completed' } })
  assert.deepEqual(activities.at(-1), ['thinking', 1])
  assert.equal(tasks[0].total, 14040)
  assert.equal(tasks[0].CNY, '0.017904')
  assert.equal(tasks[0].coverage, 'full')
  assert.equal(taskPacket(tasks[0]).trim().split('\t').length, 14)
  emit(other, 'assistant/attempt', { stream: [] })
  emit(other, 'turn/end', { reason: { kind: 'aborted' } })
  assert.equal(tasks[1].total, null)
  assert.equal(tasks[1].outcome, 'cancelled')
  assert.deepEqual(activities.at(-1), ['idle', 0])
})

test('does not use official rates or send an API key for a configured third-party endpoint', async () => {
  const ctx = new EventEmitter()
  ctx.get = service => service === 'settings'
    ? { describe: () => [{ ns: 'llm-deepseek', value: { baseURL: 'https://example.invalid' } }] } : undefined
  const read = createConnectionReader(ctx)
  assert.equal(read(provider).official, false)
  const result = await queryBalance(ctx, provider, read, new AbortController().signal, () => assert.fail('must not fetch'))
  assert.equal(result.status, 'custom')
})

test('uses the Harness credential for the official endpoint and only serializes safe balance fields', async () => {
  const key = 'fixture-key'
  const ctx = { get: name => name === 'credentials' ? { resolve: async ref => {
    assert.equal(ref, 'DEEPSEEK_API_KEY'); return { value: key }
  } } : undefined }
  const result = await queryBalance(ctx, provider, connection, new AbortController().signal, async (url, options) => {
    assert.equal(url, 'https://api.deepseek.com/user/balance')
    assert.equal(options.redirect, 'error')
    assert.equal(options.headers.Authorization, `Bearer ${key}`)
    return Response.json({ is_available: true, balance_infos: [{ currency: 'USD', total_balance: '10.25',
      granted_balance: '0.25', topped_up_balance: '10.00', extra: key }] })
  })
  assert.equal(balancePacket(result), 'balance\tready\tapi\t1\tUSD\t10.25\t0.25\t10.00\n')
})

test('signed-in platform account reports both recharge and bonus wallets with exact decimal sums', async () => {
  const ctx = { get: name => name === 'deepseekAccount' ? {
    getState: async () => ({ status: 'credential-stored' }),
    getBalance: async () => ({ status: 'ready', value: [{ currency: 'CNY', balance: '1.1' }],
      bonusWallets: [{ currency: 'CNY', balance: '2e-1' }, { currency: 'USD', balance: '0E-16' }] }),
  } : undefined }
  const result = await queryBalance(ctx, undefined, connection, new AbortController().signal, () => assert.fail('must not fetch'))
  assert.equal(balancePacket(result), 'balance\tready\taccount\t-\tCNY\t1.30\t0.20\t1.10\tUSD\t0.00\t0.00\t0.00\n')
})
