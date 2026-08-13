# Task 2 Report — 离线 PBR 像素核心

## 结论

Task 2 通过。已在 Editor 模块中实现不依赖 UObject 的离线 Metallic/Roughness 像素核心，并用 UE 5.6 / 5.7 / 5.8 的本机 `Build.bat` 完成 Development Editor 编译，再用各版本 `UnrealEditor-Cmd` 跑通 `PBRTextureLab` Automation Tests（每版本 6/6 Success，退出码 0）。未创建纹理资产、材质或 UI。

## 前置依赖

Task 1。

## 修改文件

- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabPixelCore.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabPixelCore.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabPixelCoreTests.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabEditorModule.cpp`
- `reports/checkpoint.json`

## 算法合同（确定性）

输入：`FPBRImageRgba8`（行主序、左上原点、sRGB `FColor`）。

处理：

1. **亮度**：`FLinearColor(FColor)` 走引擎 sRGB 表，再乘固定 Rec.709 系数 `0.2126 / 0.7152 / 0.0722`。不使用 `FLinearColor::GetLuminance()`（它读 CVar / working color space，非确定性）。
2. **Height**：亮度对比度 `(L-0.5)*Contrast+0.5`，可选反转，再做可分离 box blur（边缘用可用邻居均值）。
3. **Normal**：Height 上 3×3 Sobel。UE DirectX、不翻转绿通道：`N = normalize(-dX, -dY, 1)`，再 `*0.5+0.5` 编码。水平渐变（左暗右亮）中心 `R<128`；垂直渐变（上暗下亮）中心 `G<128`。
4. **AO**：半径 `{1,2,4}`、权重 `{0.50,0.35,0.15}` 的邻域高度均值；邻居更高则遮挡。
5. **Roughness**：原图亮度 3×3 标准差 `*Scale+Bias`。
6. **Metallic**：默认全黑；可选常量或亮度阈值蒙版。每次成功生成都写 `MetallicDisclaimer`，并 `UE_LOG Warning`。该通道**不是**可靠物理反演。
7. **ORM**：`R=AO, G=Roughness, B=Metallic, A=255`。
8. **BaseColor**：复制输入 RGBA 字节，不做颜色空间变换。

无 UObject、无资产 I/O、无网络、无第三方库。可在工作线程调用。

## 验证命令与结果

工作目录：`C:\Users\Administrator\Desktop\Plughins`  
工程：`PBRTextureLabHost\PBRTextureLabHost.uproject`  
工具链：VS 2022 Community，MSVC 14.44.35228，Windows SDK 10.0.22621.0

### 编译

```
"<Engine>\Engine\Build\BatchFiles\Build.bat" PBRTextureLabHostEditor Win64 Development -Project="C:\Users\Administrator\Desktop\Plughins\PBRTextureLabHost\PBRTextureLabHost.uproject" -WaitMutex
```

| 引擎 | Build.bat | 退出码 | 结果 | 耗时 | 日志 |
|---|---|---:|---|---|---|
| UE 5.8 | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 31.49s | `reports/build-ue58-task2.log` |
| UE 5.7 | `C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 34.16s | `reports/build-ue57-task2.log` |
| UE 5.6 | `C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 32.76s | `reports/build-ue56-task2.log` |

5.6 仍警告 MSVC 14.44 不是首选 14.38。编译已通过。

### Automation Tests

```
"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\Administrator\Desktop\Plughins\PBRTextureLabHost\PBRTextureLabHost.uproject" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput -AbsLog="<reports>/test-ueXX-task2.log" -ReportExportPath="<reports>/automation-ueXX" -ExecCmds="Automation RunTests PBRTextureLab;Quit"
```

套件：`PBRTextureLab.PixelCore.{SolidColor,HorizontalGradient,VerticalGradient,Checkerboard,DeterminismAndMetallic,InvalidInput}`

覆盖：尺寸、数值范围/法线半球、ORM 通道、水平/垂直法线方向、棋盘格 roughness/AO、固定输入确定性、Metallic 默认全黑 / 常量 / 阈值、非法输入拒绝。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 6 | 6 | 0 | 0 | `reports/automation-ue58/index.json`，`reports/test-ue58-task2.log` |
| UE 5.7 | 6 | 6 | 0 | 0 | `reports/automation-ue57/index.json`，`reports/test-ue57-task2.log` |
| UE 5.6 | 6 | 6 | 0 | 0 | `reports/automation-ue56/index.json`，`reports/test-ue56-task2.log` |

5.8 启动阶段出现引擎自带 `LogAutomationTest: Error: Condition failed`（早于 `Automation RunTests`）。本插件 6 个用例均为 `state: Success`，与该启动噪声无关。

## 失败项

无未解决的本插件编译失败或测试失败。

## 已知限制

- 未导入纹理资产（Task 3）。未创建材质（Task 4）。未做 UV / 菜单。
- Metallic 仅为启发式蒙版，日志与 `PBRTextureLab::MetallicDisclaimer` 已标明。
- Height / Normal / AO / Roughness 是离线估计，不是扫描或物理测量。
- 三版本顺序编译会覆盖同一 `Intermediate/` / `Binaries/`。磁盘 DLL 属于最后一次成功构建的引擎。
- Editor 无头跑测会改 `Saved/`，并可能写出 `DefaultInput.ini` / `AndroidFileServer` 配置；这些未纳入本 Task。

## 下一任务

Task 3 纹理资产导入（依赖 1+2）。Task 5 可与 Task 2 并行，现 Task 2 已通过，Task 5 可开始。
