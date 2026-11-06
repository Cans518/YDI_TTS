# 依赖与源码

应用源码采用 GPL-3.0-only；完整文本位于仓库根目录 `LICENSE`。字体、图标和框架的许可副本保存在 `resources/licenses/`，同时嵌入程序资源并随 Linux/macOS 包安装。

## Qt 6.8.2

使用以下模块的对应 6.8.2 源码。Windows 便携版静态链接；Linux/macOS 随包提供共享库。

- [Qt Base](https://download.qt.io/archive/qt/6.8/6.8.2/submodules/qtbase-everywhere-src-6.8.2.tar.xz)：Core、Gui、Widgets、Test、平台和基础图像插件。
- [Qt Serial Port](https://download.qt.io/archive/qt/6.8/6.8.2/submodules/qtserialport-everywhere-src-6.8.2.tar.xz)
- [Qt SVG](https://download.qt.io/archive/qt/6.8/6.8.2/submodules/qtsvg-everywhere-src-6.8.2.tar.xz)
- [Qt 5 Compatibility APIs](https://download.qt.io/archive/qt/6.8/6.8.2/submodules/qt5compat-everywhere-src-6.8.2.tar.xz)：Linux/macOS 的 GBK 编码。
- [Qt Wayland](https://download.qt.io/archive/qt/6.8/6.8.2/submodules/qtwayland-everywhere-src-6.8.2.tar.xz)：Linux Wayland 平台支持。

每个源码归档内包含该模块及其内置依赖的许可和版权说明。可使用修改后的对应 Qt 库重新构建本项目；全部 CMake 文件、资源、测试与打包脚本均在仓库中。

Windows 静态构建使用 MinGW 13.1 x86_64、Release、静态 Qt 和静态编译器运行库。先构建 Qt Base（`-static -static-runtime -release -opensource -confirm-license -nomake tests -nomake examples`），再用所生成的 `qt-configure-module` 构建 Serial Port 与 SVG；将安装路径传给 `scripts/package-portable.ps1 -QtRoot ...`。

## 资源

- 字体为 LXGW WenKai Mono GB Screen 1.510，采用 OFL-1.1，仓库包含原字体及许可。
- B 站与淘宝图标来自 Remix Icon 4.6.0，采用 Apache-2.0；仅调整填充颜色。
- 一点创绘官网图标取自对应网站，原始地址和处理方式见 `resources/licenses/THIRD_PARTY.md`。
