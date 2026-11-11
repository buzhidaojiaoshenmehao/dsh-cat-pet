# 小爪同伴

给 DeepSeek Harness 做的 Windows 桌宠，形象来自你的猫。它在桌面和其他普通窗口上方巡游，可以沿相邻显示器继续走。猫身以外的透明区域可以点到下方应用。

## 在 Windows 安装

1. 解压发布包。双击 `preview.cmd` 可以先体验巡游、追光标、逗猫和任务动作。预览面板标记为演示，使用示例余额和任务。
2. 启动过一次官方 DeepSeek Harness Desktop 后，从系统托盘彻底退出 Harness。
3. 在解压目录打开 PowerShell，运行下面的命令，再重新打开 Harness。桌宠会随插件启动。

```powershell
dsh plugin --profile desktop add .\dsh-cat-pet-0.2.0.tgz
```

如果终端找不到 `dsh`，在 Harness 的应用菜单启用“管理 dsh 命令”，或直接调用 Harness 安装目录中的 `resources\runtime\cli\bin\dsh.cmd`，传入相同参数。

目标宿主为官方 DeepSeek Harness `0.2.2-rc.1`。包内带 Windows x64 和 ARM64 的桌宠程序与素材，安装时无需编译。Windows ARM64 上运行 x64 版 Harness 时，插件随宿主选择 x64 桌宠。Windows 10/11 需要支持每显示器 DPI 感知的版本。

## 逗猫与账单

| 操作 | 效果 |
| --- | --- |
| 光标移动到猫附近 | 猫追上来，靠近时抬爪，在光标位置留下短暂的扑爪提示 |
| 光标停在猫身上 | 抬爪、冒爱心，停久一点会翻肚皮 |
| 左键点猫 | 打开面板，查看余额、峰谷倒计时、最近任务与设置 |
| 按住拖动 | 把猫搬到桌面上的其他位置 |
| 拖动面板里的大小滑杆 | 以 1% 为步长在 50%–180% 之间实时缩放 |
| 点面板里的音效按钮 | 切换逗猫和任务完成提示音，默认关闭 |
| 右键点猫或托盘图标 | 摸摸猫、切换音效、巡游与追光标、叫到当前屏幕、隐藏或退出 |

思考时猫会侧头，执行工具时猫会蹲下或踱步，闲置时会趴睡。任务结束会短暂弹出 Token 和预估金额。并行任务分别结算，最近一单可以在余额面板里回看。

峰谷倒计时每秒更新，显示当前高峰或空闲时段、剩余时间和下一次切换的北京时间。计费和倒计时共用同一份 2026–2027 年日历，覆盖午间、周末和节假日。日历用完时面板会提示更新。大小和音效保存在 `%LOCALAPPDATA%\dsh-cat-pet\settings.ini`。

余额优先使用当前或最近一单的 DeepSeek 路由。登录账号模式读取 Harness 自带的充值和赠送钱包，API Key 模式使用 Harness 已配置的凭证查询 DeepSeek 官方接口。尚无任务时优先查询已登录账号。自定义端点的余额由对应服务商提供。点“刷新”可重新查询，查询失败会显示原因。

用量按一轮 Harness 任务内模型请求的实际用量累计。输入包含缓存命中与未命中，推理 Token 已包含在输出里。同一轮内已经结束的本地子任务会计入父任务，父任务先结束时会标记部分统计，之后结束的子任务另行显示。第三方模型或缺少用量时只展示能核实的统计。金额按每次调用的模型、缓存及北京时间峰谷价估算，使用 2026-11-12 核对的人民币和美元价格及 2026–2027 年节假日安排。Search / Fetch 等按次收费不在 Token 估算内。

点击余额前无需额外输入 API Key。凭证留在 Harness，桌宠仅通过父子进程管道接收状态、用量和余额。鼠标互动读取位置，猫做扑爪动画。光标的移动始终由你控制。

## 卸载

从托盘退出 Harness，再运行下列命令。也可以在 Harness 的插件管理中移除“小爪同伴”。

```powershell
dsh plugin --profile desktop remove dsh-cat-pet
```

## 验证

已构建 Windows x64 和 ARM64 的 GUI 程序，并检查了压缩包和 PNG 透明度。多屏路径、计费、余额响应和任务归集测试通过。使用官方 Harness `0.2.2-rc.1` 在隔离配置里安装了插件包，实际跑完由模拟模型驱动的两次请求与一次文件读取。完成事件记录了 9,360 Token 和人民币预估费用 ¥0.005968。

Windows 窗口交互和真实账号余额仍待在 Windows 实机联调。可以先用 `preview.cmd` 检查桌宠窗口、鼠标动作和托盘菜单。

## 开发

`src/` 是 Harness 插件，`windows/` 是 Win32 桌宠，`bin/` 存放可安装的预编译程序和透明素材。用 Zig 0.14.1 构建 x64 与 ARM64。Windows 源码依赖系统 Win32 和 GDI+，发布包不需要额外的图形运行时。

```sh
node scripts/prepare-sprites.mjs
npm test
npm run build:windows
node scripts/package.mjs
```

从两张透明图集切出素材需要 `ffmpeg` 和 `ffprobe`。构建时可用 `PAW_ZIG` 指定 Zig 路径。最后一步打包命令在装有 `zip` 和 `unzip` 的 macOS 或 Linux 上运行。图集和提示词见 [动作素材说明](assets/PROMPTS.md)。

官方资料：[插件安装](https://deepseek-harness.github.io/deepseek-harness/en/develop/basic/publish) · [桌面版命令](https://github.com/deepseek-ai/deepseek-harness/blob/dsh-v0.2.2-rc.1/apps/desktop/README.md) · [余额接口](https://api-docs.deepseek.com/api/get-user-balance/) · [人民币价格](https://api-docs.deepseek.com/zh-cn/quick_start/pricing/) · [美元价格](https://api-docs.deepseek.com/quick_start/pricing/) · [2026 年假期](https://sousuo.www.gov.cn/zcwjk/policyDocumentLibrary?q=2026&t=zhengcelibrary) · [2027 年假期](https://www.gov.cn/zhengce/content/202611/content_7085101.htm)
