# 资源与依赖说明

项目源码采用根目录 `LICENSE` 中的 GPL-3.0-only。下面列出的资源与依赖按各自许可分发。

## 品牌图标

`bilibili.svg` 和 `taobao.svg` 来自 Remix Icon **v4.6.0**（Remix Design），采用 Apache License 2.0。

- Bilibili：[bilibili-fill.svg](https://github.com/Remix-Design/RemixIcon/blob/v4.6.0/icons/Logos/bilibili-fill.svg)
- 淘宝：[taobao-fill.svg](https://github.com/Remix-Design/RemixIcon/blob/v4.6.0/icons/Logos/taobao-fill.svg)
- 原始许可：[License](https://github.com/Remix-Design/RemixIcon/blob/v4.6.0/License)，完整副本见 `remixicon-LICENSE.txt`。

本项目仅修改 SVG 的填充颜色：Bilibili 为 `#df668c`，淘宝为 `#d96a29`；路径和比例保持不变。图标随程序资源打包，显示时无需联网。

## 一点创绘官网图标

`ydich.png` 取自 [一点创绘官网](https://ydich.cn/) 首页声明的 [shortcut icon](https://ydich.cn/wp-content/uploads/2025/01/2025012015415016.png)，用于官网跳转入口。

保持完整图形、原配色和比例，将原图等比缩小并居中放置在 192 × 192 的透明 PNG 画布上。图标随程序资源打包。

## 字体

`LXGWWenKaiMonoGBScreen.ttf` 为 LXGW WenKai Mono GB Screen 1.510，采用 SIL Open Font License 1.1。完整许可及版权信息见 `font-OFL.txt`，来源为 [对应版本](https://github.com/lxgw/LxgwWenKai-Screen/tree/v1.510)。未修改字体文件。

## Qt

发布包使用 Qt 6.8.2 的 Core、Gui、Widgets、SerialPort、Svg，以及 Linux/macOS 的 Core5Compat。Qt 库与插件遵循其 GPL/LGPL 许可，随附 `qt-LGPL-3.0.txt` 和项目 GPL-3.0 文本。构建与对应源码获取方式见项目文档 `docs/dependencies.md`。
