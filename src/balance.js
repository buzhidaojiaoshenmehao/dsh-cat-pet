import { accountProvider, apiProvider } from './connection.js'

const url = 'https://api.deepseek.com/user/balance'
const currencies = ['CNY', 'USD']

// Platform balances may be decimal strings in scientific notation. Use integer arithmetic.
function walletUnits(value) {
  if (typeof value !== 'string' || value.length > 64) throw new Error('invalid wallet')
  const match = /^(-?)(\d*)(?:\.(\d*))?(?:e([+-]?\d+))?$/i.exec(value)
  if (!match || !(match[2] || match[3])) throw new Error('invalid wallet')
  const digits = `${match[2]}${match[3] ?? ''}`
  const scale = 12 + Number(match[4] ?? 0) - (match[3]?.length ?? 0)
  if (!Number.isInteger(scale) || scale < -48 || scale > 32) throw new Error('invalid wallet scale')
  const units = scale < 0 ? BigInt(digits) / 10n ** BigInt(-scale) : BigInt(digits) * 10n ** BigInt(scale)
  return match[1] ? -units : units
}

function decimal(units) {
  const amount = units < 0n ? -units : units
  const fraction = (amount % 1000000000000n).toString().padStart(12, '0').replace(/0+$/, '').padEnd(2, '0')
  return `${units < 0n ? '-' : ''}${amount / 1000000000000n}.${fraction}`
}

function platformWallets(balance) {
  if (!Array.isArray(balance.value) || !Array.isArray(balance.bonusWallets)) throw new Error('invalid wallets')
  return currencies.flatMap(currency => {
    const cash = balance.value.filter(wallet => wallet.currency === currency)
    const bonus = balance.bonusWallets.filter(wallet => wallet.currency === currency)
    if (!cash.length && !bonus.length) return []
    const sum = wallets => wallets.reduce((total, wallet) => total + walletUnits(wallet.balance), 0n)
    const topped = sum(cash)
    const granted = sum(bonus)
    return [{ currency, total: decimal(topped + granted), granted: decimal(granted), topped: decimal(topped) }]
  })
}

export async function queryBalance(ctx, preferredProvider, connection, signal, fetchBalance = fetch) {
  try {
    const account = ctx.get('deepseekAccount')
    const signedIn = preferredProvider !== apiProvider && account
      && (await account.getState()).status === 'credential-stored'
    if (preferredProvider === accountProvider || signedIn) {
      if (!signedIn) return { status: 'sign_in' }
      const balance = await account.getBalance({
        version: '0.2.2-rc.1', locale: 'zh-CN', timezoneOffsetSeconds: -new Date().getTimezoneOffset() * 60,
      })
      if (balance?.status !== 'ready') return { status: balance ? 'unreachable' : 'sign_in' }
      const wallets = platformWallets(balance)
      return wallets.length ? { status: 'ready', mode: 'account', wallets } : { status: 'empty' }
    }

    const route = connection(apiProvider)
    // A custom endpoint may own the key. Only query the official API for an official route.
    if (!route.official) return { status: 'custom' }
    const ref = route.apiKeyEnv
    if (typeof ref !== 'string' || !/^[A-Za-z_][A-Za-z_0-9]*$/.test(ref)) return { status: 'no_key' }
    const credentials = ctx.get('credentials')
    const key = (await credentials?.resolve(ref))?.value
    if (!key) return { status: 'no_key' }
    const response = await fetchBalance(url, {
      headers: { Authorization: `Bearer ${key}`, Accept: 'application/json' },
      redirect: 'error', signal: AbortSignal.any([signal, AbortSignal.timeout(9000)]),
    })
    if (response.status === 401 || response.status === 403) return { status: 'auth' }
    if (!response.ok) return { status: 'unreachable' }
    const data = await response.json()
    if (!Array.isArray(data.balance_infos) || typeof data.is_available !== 'boolean') return { status: 'invalid' }
    const wallets = data.balance_infos.filter(item => currencies.includes(item.currency)).slice(0, 2).map(item => ({
      currency: item.currency, total: decimal(walletUnits(item.total_balance)),
      granted: decimal(walletUnits(item.granted_balance)), topped: decimal(walletUnits(item.topped_up_balance)),
    }))
    return wallets.length ? { status: 'ready', mode: 'api', available: data.is_available, wallets } : { status: 'empty' }
  } catch {
    return { status: 'unreachable' }
  }
}
