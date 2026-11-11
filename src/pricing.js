// Rates per million tokens, in thousandths of the stated currency.
// DeepSeek's published CNY and USD tables were checked on 2026-11-12.
const rates = {
  'deepseek-flash': { CNY: [20, 1000, 4000], USD: [3, 150, 600] },
  'deepseek-v4-pro': { CNY: [150, 4500, 13500], USD: [22, 660, 1980] },
}
const flashAliases = new Set(['deepseek-v4-flash', 'deepseek-v4-flash-vision-exp'])
const hourMs = 3600000
const dayMs = 24 * hourMs
const beijingOffset = 8 * hourMs
const firstDay = '2026-01-01'
const endDay = '2028-01-01'
const peakWindows = [[9, 12], [14, 18]]
const holidays = [
  ['2026-01-01', '2026-01-03'], ['2026-02-15', '2026-02-23'],
  ['2026-04-04', '2026-04-06'], ['2026-05-01', '2026-05-05'],
  ['2026-06-19', '2026-06-21'], ['2026-09-25', '2026-09-27'],
  ['2026-10-01', '2026-10-07'],
  ['2027-01-01', '2027-01-03'], ['2027-02-05', '2027-02-13'],
  ['2027-04-03', '2027-04-05'], ['2027-05-01', '2027-05-05'],
  ['2027-06-09', '2027-06-09'], ['2027-09-15', '2027-09-15'],
  ['2027-10-01', '2027-10-07'],
]

export function pricePeriod(at) {
  const beijing = new Date(at + beijingOffset)
  if (!Number.isFinite(beijing.getTime())) return undefined
  const date = beijing.toISOString().slice(0, 10)
  if (date < firstDay || date >= endDay) return undefined
  const day = beijing.getUTCDay()
  const hour = beijing.getUTCHours()
  const peakHours = peakWindows.some(([start, end]) => hour >= start && hour < end)
  const holiday = holidays.some(([start, end]) => date >= start && date <= end)
  return day !== 0 && day !== 6 && peakHours && !holiday ? 'peak' : 'off-peak'
}

// Every boundary at which pricePeriod changes, for the native countdown.
export function tariffTransitions() {
  const first = Date.parse(firstDay) - beijingOffset
  const end = Date.parse(endDay) - beijingOffset
  const transitions = [{ at: first, period: pricePeriod(first) }]
  for (let day = first; day < end; day += dayMs) {
    for (const hour of peakWindows.flat()) {
      const at = day + hour * hourMs
      const period = pricePeriod(at)
      if (period !== transitions.at(-1).period) transitions.push({ at, period })
    }
  }
  transitions.push({ at: end, period: undefined })
  return transitions
}

export const isCount = value => Number.isSafeInteger(value) && value >= 0

export function normalizeUsage(sample) {
  if (!sample || !isCount(sample.inputTokens) || !isCount(sample.outputTokens)) return undefined
  const { inputTokens: miss, outputTokens: output, cacheReadTokens: hit, cacheWriteTokens: write } = sample
  if ((hit !== undefined && !isCount(hit)) || (write !== undefined && !isCount(write))) return undefined
  const known = miss + (hit ?? 0) + (write ?? 0) + output
  if (!isCount(known)) return undefined
  const total = sample.totalTokens ?? known
  if (!isCount(total) || total < known || (hit !== undefined && write !== undefined && total !== known)) return undefined
  const exact = sample.totalTokens !== undefined || (hit !== undefined && write !== undefined)
  // If the cache read count is present, any uncategorized input is charged as a miss.
  const cached = hit ?? (exact && total === miss + (write ?? 0) + output ? 0 : undefined)
  return { total, input: total - output, output, cached, exact }
}

export function priceAttempt(model, usage, at) {
  const id = flashAliases.has(model) ? 'deepseek-flash' : model
  const modelRates = Object.hasOwn(rates, id) ? rates[id] : undefined
  const period = pricePeriod(at)
  if (!modelRates || !period || !usage.exact || usage.cached === undefined) return undefined
  const multiplier = period === 'peak' ? 2n : 1n
  const uncached = usage.input - usage.cached
  const estimate = ([hit, miss, output]) => multiplier * (
    BigInt(usage.cached) * BigInt(hit) + BigInt(uncached) * BigInt(miss) + BigInt(usage.output) * BigInt(output)
  )
  return { CNY: estimate(modelRates.CNY), USD: estimate(modelRates.USD) }
}

export function formatCost(nano) {
  if (nano > 0n && nano < 1000n) return '<0.000001'
  const micro = (nano + 500n) / 1000n
  const fraction = (micro % 1000000n).toString().padStart(6, '0').replace(/0+$/, '').padEnd(4, '0')
  return `${micro / 1000000n}.${fraction}`
}
