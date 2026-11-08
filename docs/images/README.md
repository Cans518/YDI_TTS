# README 界面截图

这些 PNG 来自本项目 Qt 界面的运行测试，保留原始截图，使用模拟串口 `COM7 · 长风 TTS 测试设备` 展示操作状态。

| 文件 | 原始运行产物 | 展示内容 |
| --- | --- | --- |
| `main-window.png` | `03-connected.png` | 已连接的主窗口与音量设置 |
| `sound-settings.png` | `15-sound-settings.png` | 音色与音调设置面板 |
| `read-configuration.png` | `10-read-configuration.png` | 读取模块配置后的界面 |

当前图片取自 `build/portable/verification/`，由 `tests/tst_workflows.cpp` 中的界面测试生成。首页图标直接复用 `resources/icons/cn.ydich.tts.png`。

更新界面后，在 Windows 上运行以下命令重新生成并同步截图：

```powershell
.\scripts\verify.ps1
Copy-Item build/verification/03-connected.png docs/images/main-window.png
Copy-Item build/verification/15-sound-settings.png docs/images/sound-settings.png
Copy-Item build/verification/10-read-configuration.png docs/images/read-configuration.png
```

保持图片的原始比例，并检查截图中的文字、状态与当前功能一致。截图保存在文档目录中，随仓库版本管理。
