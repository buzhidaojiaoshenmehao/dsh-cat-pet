import { mkdirSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { tariffTransitions } from '../src/pricing.js'

export function writePricingCalendar(directory) {
  const periodNames = { peak: 'peak', 'off-peak': 'offPeak' }
  mkdirSync(directory, { recursive: true })
  writeFileSync(join(directory, 'pricing-calendar.hpp'), [
    '// Generated from src/pricing.js.',
    '#pragma once',
    'namespace paw {',
    'inline constexpr TariffTransition tariffCalendar[] = {',
    ...tariffTransitions().map(({ at, period }) => `  {${at}LL, Period::${periodNames[period] ?? 'unknown'}},`),
    '};',
    '}',
    '',
  ].join('\n'))
}
