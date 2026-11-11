const publicOrigin = 'https://api.deepseek.com'
export const apiProvider = 'deepseek-official'
export const accountProvider = 'deepseek-account'

export function createConnectionReader(ctx) {
  let settings
  const invalidate = () => { settings = undefined }
  ctx.on('loader/volatile-update', invalidate)
  ctx.on('settings/document-updated', invalidate)
  return provider => {
    if (provider !== apiProvider && provider !== accountProvider) return { official: false }
    try {
      settings ??= ctx.get('settings')?.describe({ redactSecrets: true }) ?? []
      const directory = ctx.get('llm')?.listConfigurableProviders() ?? []
      const namespace = directory.find(entry => entry.provider === provider)?.settingsNs
        ?? (provider === apiProvider ? 'llm-deepseek' : 'llm-deepseek-account')
      const form = settings.find(item => item.ns === namespace)?.value
      const environment = ctx.get('launchEnvironment')?.get('DEEPSEEK_BASE_URL')?.value
      const endpoint = new URL(form?.baseURL || environment || process.env.DEEPSEEK_BASE_URL || publicOrigin)
      const apiKeyEnv = form?.apiKeyEnv ?? 'DEEPSEEK_API_KEY'
      return { official: endpoint.origin === publicOrigin, apiKeyEnv }
    } catch {
      return { official: false }
    }
  }
}
