import { formatCost, isCount, normalizeUsage, priceAttempt } from './pricing.js'
import { accountProvider, apiProvider } from './connection.js'

function usageIn(stream) {
  return stream?.findLast(record => record.type === 'chunk' && record.chunk?.type === 'usage')?.chunk.usage
}

function newTurn(number, at, owner, child) {
  return {
    number, at, owner, child, pending: new Set(), tools: new Set(), settled: new Set(), ended: false,
    total: 0, input: 0, output: 0, cached: 0, cacheUnknown: false,
    reports: 0, priced: 0, unreported: 0, CNY: 0n, USD: 0n, route: undefined,
  }
}

function merge(parent, child) {
  for (const key of ['total', 'input', 'output', 'cached', 'reports', 'priced', 'unreported', 'CNY', 'USD']) {
    parent[key] += child[key]
  }
  parent.cacheUnknown ||= child.cacheUnknown
  parent.route ??= child.route
}

function receipt(turn, reason, at) {
  const hasUsage = turn.reports > 0
  const missing = turn.unreported > 0 || turn.pending.size > 0
  const outcome = {
    completed: 'completed', 'max-tokens': 'limit', aborted: 'cancelled',
    interrupted: 'cancelled', blocked: 'blocked',
  }[reason] ?? 'failed'
  const partial = missing || turn.priced < turn.reports
  return {
    outcome, child: turn.child, at, reports: turn.reports, route: turn.route,
    total: hasUsage || !missing ? turn.total : null,
    input: hasUsage || !missing ? turn.input : null,
    output: hasUsage || !missing ? turn.output : null,
    cached: (hasUsage || !missing) && !turn.cacheUnknown ? turn.cached : null,
    incompleteTokens: missing,
    coverage: turn.priced === 0 ? 'none' : partial ? 'partial' : 'full',
    CNY: turn.priced > 0 ? formatCost(turn.CNY) : null,
    USD: turn.priced > 0 ? formatCost(turn.USD) : null,
  }
}

/** Observe live, committed events. Keep accounting fields only, never message text or arguments. */
export function observeHarness(ctx, { onActivity, onTask, connection, clock = Date.now }) {
  const active = new Map()
  const running = new Set()
  const attempts = new WeakMap()
  let previous = ''
  let balanceProvider

  function activity() {
    const count = new Set([...active.keys(), ...running]).size
    const state = [...active.values()].some(turn => turn.tools.size > 0) ? 'working'
      : count > 0 ? 'thinking' : 'idle'
    const key = `${state}:${count}`
    if (key !== previous) {
      previous = key
      onActivity(state, count)
    }
  }

  function finish(session, turn, reason, at) {
    active.delete(session.id)
    attempts.delete(session)
    turn.ended = true
    if (turn.owner && !turn.owner.ended) {
      turn.owner.pending.delete(turn)
      merge(turn.owner, turn)
      if (turn.pending.size > 0) turn.owner.unreported++
    } else {
      if (turn.route) balanceProvider = turn.route
      onTask(receipt(turn, reason, at))
    }
    activity()
  }

  ctx.on('agent/status', ({ agent, status }) => {
    if (status === 'running') running.add(agent.session.id)
    else running.delete(agent.session.id)
    activity()
  })

  ctx.on('agent/assistant-stream', ({ agent, frame }) => {
    if (frame.type !== 'start') return
    const session = agent.session
    const turn = active.get(session.id)
    if (!turn || frame.turn !== turn.number) return
    const provider = session.requestHeader()?.config?.provider
    attempts.set(session, { at: clock(), provider, official: connection(provider).official })
    if (!turn.child && (provider === apiProvider || provider === accountProvider)) balanceProvider = provider
  })

  ctx.on('session/disposed', session => {
    running.delete(session.id)
    const turn = active.get(session.id)
    if (turn) {
      turn.unreported++
      finish(session, turn, 'interrupted', clock())
    }
    activity()
  })

  ctx.on('session/event', (session, event) => {
    const { type } = event
    const data = event.data ?? {}
    const at = isCount(event.time) ? event.time : clock()
    if (type === 'turn/start') {
      if (active.has(session.id)) return
      const child = session.header?.origin === 'subagent'
      const owner = child ? active.get(session.header.parentSession) : undefined
      const turn = newTurn(data.turn, at, owner, child)
      active.set(session.id, turn)
      owner?.pending.add(turn)
      activity()
      return
    }

    const turn = active.get(session.id)
    if (!turn) return
    if (type === 'subagent/catalog' && data.mode === 'external') turn.unreported++
    if (data.turn !== turn.number) return

    switch (type) {
      case 'step/start':
      case 'llm/retry-started':
        turn.at = at
        attempts.delete(session)
        break
      case 'tool/call':
        turn.tools.add(data.callId)
        activity()
        break
      case 'tool/result':
        turn.tools.delete(data.message.toolCallId)
        activity()
        break
      case 'assistant/message':
      case 'assistant/attempt': {
        if (turn.settled.has(event.seq)) break
        turn.settled.add(event.seq)
        const attempt = attempts.get(session)
        attempts.delete(session)
        const source = data.message?.source ?? session.requestHeader()?.config
        if (source?.provider === apiProvider || source?.provider === accountProvider) turn.route = source.provider
        const usage = normalizeUsage(data.usage ?? usageIn(data.stream))
        if (!usage) { turn.unreported++; break }
        turn.reports++
        turn.total += usage.total
        turn.input += usage.input
        turn.output += usage.output
        turn.cached += usage.cached ?? 0
        turn.cacheUnknown ||= usage.cached === undefined
        if (!usage.exact) turn.unreported++
        const official = attempt && attempt.provider === source?.provider
          ? attempt.official : connection(source?.provider).official
        const cost = official ? priceAttempt(source.model, usage, attempt?.at ?? turn.at) : undefined
        if (cost) {
          turn.priced++
          turn.CNY += cost.CNY
          turn.USD += cost.USD
        }
        break
      }
      case 'turn/end':
        finish(session, turn, data.reason?.kind, at)
        break
    }
  })

  activity()
  return { get balanceProvider() { return balanceProvider } }
}
