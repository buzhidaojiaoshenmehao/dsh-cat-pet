# 小爪同伴

给 DeepSeek Harness 做的 macOS 和 Windows 桌宠。它会在桌面巡游，随任务切换动作。点击猫可以查看余额、峰谷倒计时、最近任务的 Token 和预估费用。

## 在 macOS 安装

支持 macOS 13 或更新版本，包括 Apple 芯片和 Intel Mac。至少打开过一次 DeepSeek Harness Desktop 后，用 `⌘Q` 退出，再在终端运行以下命令。

```sh
'/Applications/DeepSeek Harness.app/Contents/Resources/runtime/cli/bin/dsh' plugin --profile desktop add 'https://github.com/buzhidaojiaoshenmehao/dsh-cat-pet/releases/download/v0.6.0/dsh-cat-pet-0.6.0.tgz'
```

重新打开 Harness 后，小爪同伴会随插件启动。点击菜单栏的脚印可以打开面板、召回、隐藏或退出桌宠。Harness 安装在其他位置时，将命令里的应用路径改成实际位置。

## 在 Windows 安装

1. 解压发布包。双击 `preview.cmd` 可以先体验巡游、追光标、逗猫和任务动作。预览面板标记为演示，使用示例余额和任务。
2. 启动过一次官方 DeepSeek Harness Desktop 后，从系统托盘彻底退出 Harness。
3. 在解压目录打开 PowerShell，运行下面的命令，再重新打开 Harness。桌宠会随插件启动。

```powershell
dsh plugin --profile desktop add .\dsh-cat-pet-0.6.0.tgz
```

如果终端找不到 `dsh`，在 Harness 的应用菜单启用“管理 dsh 命令”，或直接调用 Harness 安装目录中的 `resources\runtime\cli\bin\dsh.cmd`，传入相同参数。

已在官方 DeepSeek Harness `0.2.2-rc.1` 上验证安装。同一个 `.tgz` 包含 macOS 通用程序、Windows x64 和 ARM64 程序及素材，安装时无需编译。Windows ARM64 上运行 x64 版 Harness 时，插件随宿主选择 x64 桌宠。Windows 10/11 需要支持每显示器 DPI 感知的版本。

## 逗猫与账单

| 操作 | 效果 |
| --- | --- |
| 光标移动到猫附近 | 猫追上来，靠近时抬爪，在光标位置留下短暂的扑爪提示 |
| 光标停在猫身上 | 抬爪、冒爱心，停久一点会翻肚皮 |
| 左键点猫 | 在猫旁边打开余额、峰谷倒计时、最近任务与设置气泡 |
| 鼠标移进气泡 | 气泡保持打开。离开 5 秒后收起，期间移回会取消计时 |
| 按住拖动 | 把猫搬到桌面上的其他位置 |
| 拖动面板里的大小滑杆 | 以 1% 为步长在 50%–180% 之间实时缩放 |
| 关闭自动巡游 | 停止自动在桌面走动。追光标可在脚印菜单里单独设置 |
| 打开音效 | 点击猫、摸猫或扑到光标时会听到一声猫叫，任务完成仍会播放提示音 |
| 点击最近任务右侧箭头 | 展开输入、输出、缓存命中和费用说明 |
| 右键点猫，或打开 macOS 菜单栏或 Windows 托盘里的脚印 | 摸摸猫、让猫伸懒腰、切换音效和巡游、叫到当前屏幕、隐藏或退出 |

走路时四条腿依次迈步。伸懒腰时，猫会伸出前爪、压低胸口，停一拍再起身。猫在静坐时会眨眼、呼吸，思考时会侧头，执行工具时会蹲下或踱步，闲置时会趴睡。任务结束会短暂弹出 Token 和预估金额。音效默认关闭，开启时会试听一声猫叫。 [音频来源](assets/AUDIO.md)

峰谷倒计时每秒更新。高峰用红色，空闲用绿色，下一次切换时间以北京时间显示。计费和倒计时共用同一份 2026–2027 年日历，覆盖午间、周末和节假日。日历用完时面板会提示更新。macOS 将大小、音效和巡游设置写入系统偏好设置，Windows 保存在 `%LOCALAPPDATA%\dsh-cat-pet\settings.ini`。

余额优先使用当前或最近一单的 DeepSeek 路由。登录账号模式读取 Harness 自带的充值和赠送钱包，API Key 模式使用 Harness 已配置的凭证查询 DeepSeek 官方接口。尚无任务时优先查询已登录账号。自定义端点的余额由对应服务商提供。点“刷新”可重新查询，查询失败会显示原因。

用量按一轮 Harness 任务内模型请求的实际用量累计。输入包含缓存命中与未命中，推理 Token 已包含在输出里。同一轮内已经结束的本地子任务会计入父任务，父任务先结束时会标记部分统计，之后结束的子任务另行显示。第三方模型或缺少用量时只展示能核实的统计。金额按每次调用的模型、缓存及北京时间峰谷价估算，使用 2026-11-12 核对的人民币和美元价格及 2026–2027 年节假日安排。Search / Fetch 等按次收费不在 Token 估算内。

点击余额前无需额外输入 API Key。凭证留在 Harness，桌宠仅通过父子进程管道接收状态、用量和余额。鼠标互动读取位置，猫做扑爪动画。光标的移动始终由你控制。

## 卸载

在 Harness 的插件管理中移除“小爪同伴”。也可以完全退出 Harness 后，使用它自带的 `dsh` 运行下列命令。

```sh
dsh plugin --profile desktop remove dsh-cat-pet
```

## 验证

已构建 macOS 通用程序和 Windows x64、ARM64 程序。多屏路径、计费、余额响应和任务归集测试通过。在官方 Harness `0.2.2-rc.1` 的隔离配置中安装插件，用模拟模型测试真实工具和任务事件。

在 Apple 芯片 Mac 上实测了桌宠窗口、面板、大小滑杆和音效开关。余额与用量通过模拟数据验证。Windows 可先用 `preview.cmd` 检查桌宠窗口、鼠标动作和托盘菜单。

## 开发

`src/` 是 Harness 插件，`macos/` 和 `windows/` 分别使用 AppKit 和 Win32，`native/` 提供共享的路径、计费和协议代码。`bin/` 存放预编译程序和透明素材。完整发布包在装有 Xcode 或 Command Line Tools、Zig 0.14.1、`zip` 和 `unzip` 的 Mac 上构建。

```sh
node scripts/prepare-sprites.mjs
npm test
npm run build:macos
npm run build:windows
node scripts/package.mjs
```

切分透明图集需要 `ffmpeg` 和 `ffprobe`。如需重制四足步态与伸懒腰，先切分图集，再用装有 `opencv-python-headless` 的 Python 运行 `python scripts/prepare-walk.py`。构建时可用 `PAW_ZIG` 指定 Zig 路径。构建后可以从项目目录运行 `open bin/macos/LittlePaw.app` 体验演示。图集和提示词见 [动作素材说明](assets/PROMPTS.md)。

官方资料：[插件安装](https://deepseek-harness.github.io/deepseek-harness/en/develop/basic/publish) · [桌面版命令](https://github.com/deepseek-ai/deepseek-harness/blob/dsh-v0.2.2-rc.1/apps/desktop/README.md) · [余额接口](https://api-docs.deepseek.com/api/get-user-balance/) · [人民币价格](https://api-docs.deepseek.com/zh-cn/quick_start/pricing/) · [美元价格](https://api-docs.deepseek.com/quick_start/pricing/) · [2026 年假期](https://sousuo.www.gov.cn/zcwjk/policyDocumentLibrary?q=2026&t=zhengcelibrary) · [2027 年假期](https://www.gov.cn/zhengce/content/202611/content_7085101.htm)
