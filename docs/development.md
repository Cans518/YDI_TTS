# 开发指南

## 代码组织

```text
cmake/                     编译选项、静态插件与安装规则
src/
  CMakeLists.txt           定义核心、界面和应用库
  core/                    文本编码、帧格式与异步事务
  ui/                      控件、主题、动画与弹出面板
  app/
    main.cpp               应用入口
    serialsender.h         主窗口接口与运行状态
    serialsender.cpp       设备连接、操作提交与结果处理
    serialsender_view.cpp  布局、绘制、拖动与界面状态更新
    staticplugins.cpp      单文件版 Qt 插件注册
tests/
  ttstests.h               测试槽声明与界面测试辅助方法
  tst_tts.cpp              测试入口、初始化与清理
  tst_protocol.cpp         编码、参数映射与串口事务测试
  tst_widgets.cpp          控件、动画和提示栏测试
  tst_workflows.cpp        主窗口交互与完整操作流程测试
  support/                 模拟串口与 URL 捕获工具
scripts/                   构建、打包、发布及清理入口
resources/                 随应用嵌入的资源及许可
packaging/                 平台分发资源
docs/                     使用与开发文档、README 截图
```

CMake 依赖方向为 `YDI_TTS → tts_app → tts_ui → tts_core`。协议核心只依赖 Qt Core（非 Windows 的 GBK 转换另用 Core5Compat）；串口设备由应用层通过 QIODevice 接口交给核心事务类。通用控件不依赖主窗口。

主窗口的设备与操作流程放在 `serialsender.cpp`，布局和显示行为放在 `serialsender_view.cpp`。两者实现同一个类，控件生命周期和信号连接由主窗口统一管理。

## 构建与验证

Windows 使用 PowerShell 7：

```powershell
# 普通构建、完整测试、运行库打包与独立启动检查
.\scripts\verify.ps1 -Package

# 使用静态 Qt 构建并验证单文件版
.\scripts\package-portable.ps1
```

两种构建的默认临时目录分别为 `%TEMP%/ydi-tts-verify` 和 `%TEMP%/ydi-tts-portable`，可通过 `-BuildRoot` 指定。源码暂存范围为根目录 CMake 文件、LICENSE，以及 `cmake/`、`src/`、`resources/`、`tests/`、`packaging/`。

测试保留统一的 `tts_tests` 入口和测试槽顺序。可以通过 Qt Test 的函数参数运行指定用例，也可以使用 CTest 运行全部测试：

```sh
ctest --test-dir build/local --output-on-failure
```

自动化测试使用模拟串口；设备联调时再确认实际模块的播报、参数回读与插拔行为。构建输出及测试报告在 `build/`，公开展示的截图保存在 `docs/images/`，更新步骤见 [截图说明](images/README.md)。

Linux/macOS 的构建与发布流程见 [跨平台打包](packaging.md)。

## 清理本地产物

```powershell
# 预览清理范围
.\scripts\clean.ps1 -Scope All -WhatIf

# 清理工作区构建产物及默认临时构建目录
.\scripts\clean.ps1 -Scope All

# 仅清理临时编译目录，保留工作区中生成的可运行文件与报告
.\scripts\clean.ps1 -Scope Temporary
```

`Workspace`（默认）清理根目录 `build/`、`out/`、`dist/` 和 `scripts/`、`tests/` 下的 Python 缓存；`Temporary` 清理上述两个默认临时构建目录。`All` 包含两者。清理前请结束正在使用这些目录的构建或程序，并将需要保留的发布文件移出产物目录。自定义 `-BuildRoot` 目录由使用者单独管理。

源码、文档截图、工具链、Git 历史及 IDE 个人设置不属于清理范围。

## 修改约定

- 新增源文件时，在对应 CMake 目标中显式登记；新增构建支持目录时同步更新 `Copy-BuildSources`。
- 测试按职责放入对应文件，在 `ttstests.h` 声明测试槽；公共测试替身放入 `tests/support/`。
- 使用 UTF-8、LF 和根目录 `.clang-format`。新增资源同步检查资源清单及许可说明。


## 状态与生命周期约定

- `SerialSender` 接管传入串口的所有权；`SerialTransaction` 借用设备，通过 `QPointer` 观察其生命周期。设备关闭或销毁会取消当前事务。
- `start()` 返回是否接受整批命令。空指令或不可读写的设备不会启动事务；操作结果通过 `finished()` 返回，写入失败可能在 `start()` 返回前同步通知。
- 进度信号发出时指令尚未写入，只有进入写入阶段才接收确认。代际编号使取消、重试之前排队的回调失效，帧间隔在这些操作之间保持连续。
- `currentConfiguration` 保存已确认参数；`submittedConfiguration` 保存待提交参数。只有完整操作成功才更新已确认值。断开连接时通过 `disconnecting` 避免同步取消信号再次进入错误处理。
- 参数档位以 `ttsprotocol.h` 的映射表为准；距离和滑块范围计算先提升到 64 位，避免极值运算溢出。

这些约定分别由事务生命周期、重入回调、参数边界及窗口工作流测试覆盖。测试槽仅属于 `tts_tests` 目标，不参与应用目标的编译和分发。
