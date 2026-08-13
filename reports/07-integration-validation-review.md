# Task 7 Report — 集成、三版本验证与独立审查

## 结论

Task 7 通过。已增加 Nomad Tab：可选 `Texture2D` 或本地图片，配置输出目录/尺寸/参数，预览，并执行 Generate → Import → MIC。Tab 内接入 UV 100×/500× 命令。UE 5.6 / 5.7 / 5.8 Development Editor 编译退出码 0；`PBRTextureLab` Automation 各 30/30 Success；各版本第二次 Editor 进程 `PBRTextureLab.Integration.ReloadAfterRestart` 均为 1/1 Success。同进程集成测试覆盖真实 UV 100×/500× 与 Undo/Redo。这不是完整 OpenPBR。

## 前置依赖

Task 4、Task 6（并复用 Task 2 像素核心、Task 3 导入、Task 5 UV 预设）。

## 修改文件

- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabPipeline.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabPipeline.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabNomad.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabNomad.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/SPBRTextureLabNomad.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/SPBRTextureLabNomad.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabPipelineTests.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabCommands.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabCommands.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabEditorModule.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabEditorModule.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/PBRTextureLabEditor.Build.cs`（`WorkspaceMenuStructure` / `DesktopPlatform` / `PropertyEditor`）
- `reports/checkpoint.json`

Editor 写出的 `DefaultEngine.ini` / `DefaultInput.ini` 已丢弃，未纳入本次改动。

## 集成合同

- Nomad Tab id：`PBRTextureLabNomad`。`FGlobalTabmanager::RegisterNomadTabSpawner` 挂在 `WorkspaceMenu::GetToolsCategory()`。命令 `PBRTextureLab.OpenNomadTab` 无默认快捷键，出现在 Tools 菜单和 User 工具栏。
- 源：`SObjectPropertyEntryBox` 选 `UTexture2D`；Content Browser 当前选中的 Texture2D；`IDesktopPlatform::OpenFileDialog` 选本地 png/jpg/bmp/tga。
- 读源用本机 `FImageUtils::GetTexture2DSourceImage` / `DecompressImage`，转 BGRA8 sRGB 后交给 Task 2 `GeneratePBRMaps`。
- 输出：`/Game` 目录、BaseName、宽高（0 = 源尺寸，上限 4096）、冲突策略默认 `Cancel`（不覆盖）。`Replace` / `UniqueName` 需显式选择。
- 参数：HeightContrast/Blur/Invert、NormalStrength、RoughnessScale/Bias、MetallicMode、MIC 的 HeightAmount / UVScale。界面展示 Metallic 免责声明。
- 预览：`GeneratePBRMaps` 后用 `FSlateDynamicImageBrush::CreateWithImageData` 显示 Source/BaseColor/Normal/Roughness/Metallic/ORM。
- 生成：`GenerateAndImportFromSource` 必须在 Game Thread 调用；导入与 MIC 复用 Task 3/4。
- UV：Tab 内按钮调用既有 `ExecuteRegisteredUVCommand`（复制/改源/取消语义不变）。

## 验证命令与结果

工作目录：`C:\Users\Administrator\Desktop\Plughins`  
工程：`PBRTextureLabHost\PBRTextureLabHost.uproject`  
工具链：VS 2022 Community，MSVC 14.44.35228，Windows SDK 10.0.22621.0

跨版本共享 `Intermediate/` 时，较新/较旧引擎会吃到另一版本的 UHT 头。换引擎前已删除 Host/插件 `Intermediate/` 再编。

### 编译

```
"<Engine>\Engine\Build\BatchFiles\Build.bat" PBRTextureLabHostEditor Win64 Development -Project="C:\Users\Administrator\Desktop\Plughins\PBRTextureLabHost\PBRTextureLabHost.uproject" -WaitMutex
```

| 引擎 | Build.bat | 退出码 | 结果 | 耗时 | 日志 |
|---|---|---:|---|---|---|
| UE 5.8 | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 38.57s（清 Intermediate 后全量） | `reports/build-ue58-task7.log` |
| UE 5.7 | `C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 36.59s | `reports/build-ue57-task7.log` |
| UE 5.6 | `C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 35.74s | `reports/build-ue56-task7.log` |

5.6 仍警告 MSVC 14.44 不是首选 14.38。编译已通过。

5.8 第一次清 Intermediate 编译在测试里用了 `TSharedRef::IsValid()`（该接口为 private）失败；改掉后增量通过。上表 38.57s 是最终代码的第二次清 Intermediate 全量成功编译。

### Automation Tests（同进程）

```
"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "...\PBRTextureLabHost.uproject" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput -AbsLog="<reports>/test-ueXX-task7.log" -ReportExportPath="<reports>/automation-ueXX-task7" -ExecCmds="Automation RunTests PBRTextureLab;Quit"
```

套件：既有 24 个用例 + `PBRTextureLab.Pipeline.{FromTexture2D,FromLocalFile,CancelAndInvalid}` + `PBRTextureLab.Nomad.TabRegistered` + `PBRTextureLab.Integration.{GenerateUVUndoRedo,ReloadAfterRestart}`。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 30 | 30 | 0 | 0 | `reports/automation-ue58-task7/index.json` |
| UE 5.7 | 30 | 30 | 0 | 0 | `reports/automation-ue57-task7/index.json` |
| UE 5.6 | 30 | 30 | 0 | 0 | `reports/automation-ue56-task7/index.json` |

覆盖：Texture2D 源读回并缩放到 8×8 后导入+MIC；本地 PNG 解码生成；取消/空源/缺文件/超尺寸失败且不写资产；Nomad Tab 已注册、`OpenNomadTab` 无默认和弦、可 `TryInvokeTab`；集成路径生成 MIC 后对网格做 100×→500×、Undo 回 100、Redo 到 500，再保存 100×。

### Editor 重启后加载（第二进程）

先跑完同进程套件并退出，再启动新的 `UnrealEditor-Cmd`：

```
-ExecCmds="Automation RunTests PBRTextureLab.Integration.ReloadAfterRestart;Quit"
```

读取 `/Game/PBRTextureLab/Automation/T7Inst<主版本><次版本>_Inst` 与 `T7UV<主版本><次版本>`。日志中有 `FlushAsyncLoading`。核验 MIC 父材质与贴图绑定、`NormalStrength=0.75`、`UVScale=2`、网格 `AppliedScale==100` 且第二顶点 UV0.X==25。

| 引擎 | 发现 | 通过 | 失败 | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---|
| UE 5.8 | 1 | 1 | 0 | 0 | `reports/automation-ue58-task7-restart/index.json` |
| UE 5.7 | 1 | 1 | 0 | 0 | `reports/automation-ue57-task7-restart/index.json` |
| UE 5.6 | 1 | 1 | 0 | 0 | `reports/automation-ue56-task7-restart/index.json` |

## 独立审查

独立子代理只依据 `tasks/07`、`AGENTS.md`、未提交插件源码和本报告列出的构建/测试产物做合规与质量两遍审查，未提供实现摘要。

### Pass A — 合规

| 需求 | 状态 | 证据 |
|---|---|---|
| Nomad Tab | 已实现 | `RegisterNomadTabSpawner` + `SPBRTextureLabNomad`；`Nomad.TabRegistered` 三版本 Success |
| Texture2D 或本地图 | 已实现 | `FromTexture2D` / `FromLocalFile` 三版本 Success |
| 输出目录/尺寸/参数 | 已实现 | 内容路径、宽高、像素/MIC 参数；测试覆盖缩放与标量 |
| 预览 | 代码已实现 / 运行时显示未验证 | `OnRefreshPreview` + Slate brush；无用例点 Preview 或读 brush 像素 |
| 执行生成 | 已实现 | `GenerateAndImportFromSource`；管线与集成用例三版本 Success |
| 接入 UV 命令 | 已实现 | Tab 按钮调用 `ExecuteRegisteredUVCommand`；100/500/Undo/Redo 由 `ApplyUVPreset` 测到 |
| 5.6/5.7/5.8 编译 | 已实现 | 三份 `Result: Succeeded` 日志 |
| Automation | 已实现 | 各 30/30，Editor 退出码 0 |
| Editor 重启加载 | 已实现 | 第二进程 `Integration.ReloadAfterRestart` 各 1/1 |
| 真实 UV 100×/500×、Undo/Redo | 已实现 | `Integration.GenerateUVUndoRedo` + 既有 `UV.UndoRedo` |
| Editor-only / Game Thread / 非 OpenPBR | 已实现 | `uplugin` Editor 模块；`GenerateAndImportFromSource` 检查 Game Thread |

审查员当时读到的 5.8 构建日志是修复测试后的增量 4 actions。之后已再清 Intermediate 全量重编 5.8，退出码 0，38.57s，见当前 `reports/build-ue58-task7.log`。

### Pass B — 质量

Critical / High（P0/P1）：0。

- **P2**：Nomad UV 按钮在空选/取消后仍写 “command dispatched”，不回报真实状态。
- **P2**：冲突策略选 Replace 后立刻覆盖，没有二次列出将替换的资产。默认仍是 Cancel。
- **P3**：`bBusy` 未赋值；预览缩略图宽高各自 clamp 到 128，可能改纵横比。

裁决：**带披露限制通过**。未修 P2/P3，避免改完再废掉已跑完的三版本证据。

### 审查员标明的未验证项

- 交互点击 Preview / Generate / UV 按钮本身（同一 API 已测）。
- 从 Nomad 走 Task 6 复制/改源/取消对话框。
- 贴图+MIC 一次 Undo。
- 除 PNG 以外的本地格式；无 source art 的 Texture2D。

## 失败项

无（最终代码）。中间 5.8 测试编译错误 `TSharedRef::IsValid` 为 private，已改为校验 Tab 标签，未进入提交。

## 已知限制

- Nomad Tab 的鼠标点选是在 `-NullRHI -unattended` 下通过注册、`TryInvokeTab` 和与 UI 相同的 `GenerateAndImportFromSource` 验证的。未开带窗口的交互 Editor 做人工点击。
- UI 预览与生成在 Game Thread 跑像素核心。大图可能卡 Editor；像素计算本身仍无 UObject。
- 无人值守下本地文件对话框不会弹出；测试写临时 PNG 直接走 `ReadSourceLocalFile`。
- 冲突策略默认 Cancel。用户在 UI 里选 Replace 才会覆盖，且没有二次确认列表。
- Nomad UV 按钮状态文案不区分空选/取消/成功。
- 预览缩略图最多 128，宽高分别限制。
- 共享 `Intermediate/` 的 UHT 头不能跨 5.6/5.7/5.8 使用。
- 5.6 加载同目录下 5.7/5.8 自动化包会打 “Package is unloadable”；用例按引擎次版本分名，测试仍通过。
- 仍不是完整 OpenPBR。Metallic 仍是启发式蒙版。
- Connecter 插件搬家仍未做。
