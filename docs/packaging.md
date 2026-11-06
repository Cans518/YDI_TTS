# 跨平台打包

版本号统一取自根目录 CMake 文件。使用 Qt 6.8.2、CMake 3.19+、C++17 编译器与 Python 3.10+。Qt 模块包括 Core、Gui、Widgets、SerialPort、Svg 和 Test；Linux/macOS 还需要 Core5Compat，Linux 包另带 Wayland 支持。

## Windows

```powershell
.\scripts\package-portable.ps1
```

默认使用本机静态 Qt 及 MinGW 13.1，参数可覆盖工具链位置。输出 `build/portable/release/YDI_TTS_Portable.exe`。发布前检查导入表、完整自动测试、Windows/offscreen 独立启动及无附带配置文件。

GitHub Actions 同时使用 MSVC 编译并验证 Windows 源码。单文件 EXE 使用上述静态构建流程生成，在发布草稿确认后上传。

## Linux x86_64

安装 Qt 及系统图形开发依赖、Ninja、patchelf、desktop-file-utils、Xvfb、Weston。

```bash
export QT_ROOT_DIR=/path/to/Qt/6.8.2/gcc_64
bash scripts/package-linux.sh
```

脚本编译并运行测试，安装到临时 AppDir，下载官方 linuxdeploy 和 Qt 插件并校验下载摘要，生成 `dist/YDI_TTS-版本-linux-x86_64.AppImage`。然后检查依赖路径，并验证 offscreen、X11 和可用的 Wayland 启动。CI 固定使用 Ubuntu 22.04，避免意外提高系统兼容基线。

运行 AppImage：

```bash
chmod +x YDI_TTS-*-linux-x86_64.AppImage
./YDI_TTS-*-linux-x86_64.AppImage
```

系统未提供 FUSE 时，可使用 `APPIMAGE_EXTRACT_AND_RUN=1 ./文件.AppImage`。串口设备的访问权限应按所用发行版配置，例如所属用户组或 udev 规则。

## macOS

在 macOS 上安装 Xcode Command Line Tools、Qt、CMake 和 Ninja。分别在 arm64 与 x86_64 构建环境运行：

```bash
export QT_ROOT_DIR=/path/to/Qt/6.8.2/macos
bash scripts/package-macos.sh
```

脚本设置 macOS 12.0 最低目标版本，生成应用包、部署 Qt 框架与 Cocoa 插件、进行本地临时签名、验证依赖和独立启动，并创建包含 Applications 快捷方式的 DMG。

当前发布包明确标注 `unnotarized`，没有 Developer ID 签名或公证。首次运行可能被系统拦截，应先核对发布来源与文件摘要，再按系统“隐私与安全性”提示处理。正式签名、公证需要另行配置有效的开发者身份。

## GitHub 发布流程

`Build and package` 工作流验证 Windows、Linux x86_64、macOS arm64、macOS x86_64。普通推送生成测试证据和平台包；手动触发时提供与 CMake 版本一致的 `release_tag`（例如 `v1.2.0`），全部通过后生成发布草稿。

1. 确认各平台构建、测试及打包启动成功。
2. 用同一源码提交构建 Windows 便携 EXE，上传至草稿。
3. 汇总全部发布文件，用 `scripts/release-metadata.py manifest 目录` 生成最终校验和及提交记录。
4. 检查签名状态与版本说明后发布草稿。

CI 自动验证不替代真实串口硬件验收；USB 串口驱动、设备权限、配置回读及播报应在目标设备上确认。
