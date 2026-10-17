# GameScrcpy 宣传片（Remotion 工程）

1920×1080 · 30fps · 2216 帧（≈1 分 14 秒）· H.264 + AAC 立体声 · 中文屏幕文案 · 无配音

成片：`out/GameScrcpy-promo-1080p.mp4`

## 结构

真机演示放在开头第二个场景（原视频里在尾部），键位映射拆成「总览 + 细节」两幕。

| 场景 | 帧区间 | 内容 |
| --- | --- | --- |
| `Title` | 0–165 | Logo + 字标 + 一句话定位 |
| `UiTour` | 153–525 | **真机界面巡游**：首页（USB/WiFi/刷新，光标点击 + 涟漪 + 扫光）→ 擦除切到设置（视频参数/显示选项 → 滚动到通道控制/触控设置/外观）→ 擦除切到 ADB 终端 |
| `Keymap` | 513–813 | 键位映射总览：**横屏 16:9 编辑器窗口**（940×564，与产品内实际编辑画布一致）+ 左侧 NODES 节点面板；画布上 WASD 循环按下、steerWheel 摇杆、click 涟漪、drag 轨迹、freeLook 选区、FIRE + 压枪下沉 |
| `KeymapDeep` | 801–1271 | 键位映射细节：六类节点清单（click/steerWheel/drag/freeLook/script/recoilAssist 各带微动效图标）+ 辅助压枪曲线（无辅助漂移 vs recoilAssist 压回，16 项参数刻度逐个点亮）+ 组合键 Shift+G + `keymap.json` 热加载 |
| `Stream` | 1259–1484 | 投屏链路：手机 → 信号 → 镜像窗口，窗口内含滚动延迟示波器（LATENCY 实时毫秒 + 波形），KCP/FEC/ABR |
| `Script` | 1472–1682 | QuickJS 脚本：逐字打字 + 行高亮 + toast |
| `Vision` | 1670–1870 | 图像识别选区扫描 + 拟人化轨迹（渐进描边 + 笔头光点） |
| `Backends` | 1858–2053 | 四条触控注入链路（ADB/UHID/AOA/ESP32），卡片内链路脉冲 |
| `Outro` | 2041–2216 | 收尾 + 淡黑 |

场景之间为 12 帧交叉淡入（`FADE` in `src/Main.tsx`），因此相邻帧区间有重叠。改时长只需动 `SCENES` 里的 `dur`，`TOTAL` 由它推导。

## 音频

`public/bgm.wav` 由 `scripts/make-bgm.mjs` 离线合成（无版权素材）：A 小调 72BPM，Am–F–C–G 循环 pad + sub bass + 稀疏 pluck + 极轻心跳 thump + air 层。`Main.tsx` 用 `<Audio>` 挂载，首 42 帧淡入到 0.52、末尾 90 帧淡出。

```bash
node scripts/make-bgm.mjs 92   # 参数为秒数，必须长于成片（当前 74s，留 18s 余量）
```

## 真实界面截图

`out/ui/page_{home,settings,settings_scrolled,terminal}.png`（1300×740，`PrintWindow` + `PW_RENDERFULLCONTENT` 采集），同名文件在 `public/ui/` 供 Remotion 读取。采集脚本 `scripts/capture-pages.ps1`（纯 ASCII，避免 PowerShell 5.1 按 GBK 读无 BOM 脚本把中文字面量弄坏；按 UIA Button 索引切页 1=首页 2=设置 3=终端 4=帮助；`WM_MOUSEWHEEL` 抓滚动态）。元素矩形清单见 `out/ui/ui_manifest.json`，视频内标注坐标全部来自该清单。

## 常用命令

```bash
# 预览（浏览器编辑器）
node node_modules/@remotion/cli/remotion-cli.js studio

# 渲染整片
node node_modules/@remotion/cli/remotion-cli.js render GameScrcpy out/GameScrcpy-promo-1080p.mp4

# 单帧检查
node node_modules/@remotion/cli/remotion-cli.js still GameScrcpy out/s150.png --frame=150

# 静帧数值体检（亮度、文字包围盒、强调色占比）
node scripts/inspect.mjs out/s150.png

# 截图互异体检（magenta 残留 / 亮度签名）
node scripts/verify-ui.mjs out/ui/*.png
```

## 两个环境坑（本机 ver 4.0.532）

1. **必须用 `remotion-cli.js` 作为入口**：`node_modules/@remotion/cli/dist/index.js` 只是库入口，直接执行静默无输出。
2. **stich 阶段 ffmpeg 报 `Permission denied`**：默认把中间文件写到 `%TEMP%\remotion-*\...\fast-start-intermediate`，在受管环境下会被拒绝。渲染前把 TEMP 指到工程内即可：

   ```powershell
   $env:TEMP = (Resolve-Path out\tmp).Path; $env:TMP = $env:TEMP
   ```

   帧序列本身不受影响（此时日志会显示 2216/2216 已渲染，仅封装失败）。

## 视觉基调

单一强调色（indigo `#6366F1`）+ zinc 中性灰，取自产品自身 Qt 主题 `client/src/ui/resources/style/modern_dark.qss`；字体只用系统字体，无外链资源。所有配色/缓动集中在 `src/theme.ts`，通用动效件在 `src/components/kit.tsx`。
