# Overnight P1 — UE 5.6 / 5.7 / 5.8 回归

日期：2026-08-27  
工程：`PBRTextureLabHost/PBRTextureLabHost.uproject`  
工作目录：`C:\Users\Administrator\Desktop\UE-PBRUV-develop\UE-PBRUV-develop`  
套件：`Automation RunTests PBRTextureLab`（37 项，含新增 `PBRTextureLab.Preview.Primitives`）  
第二进程：`Import|Material|UV|Integration.ReloadAfterRestart` 各 1/1

## 结论

三版本 Development Editor **编译退出码 0**，Automation **失败项 0**。5.8 最终 37/37 Success；5.7 35 Success + 2 with warnings；5.6 34 Success + 3 with warnings。这不是完整 OpenPBR。

证据目录：`reports/overnight-20260827/`。

## 本机引擎

| 引擎 | 版本标识 | Build.bat | UnrealEditor-Cmd |
|---|---|---|---|
| UE 5.8 | 5.8.1-56057345 `++UE5+Release-5.8` | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | `...\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` |
| UE 5.7 | 5.7.4-51494982 `++UE5+Release-5.7` | `C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat` | `...\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` |
| UE 5.6 | `++UE5+Release-5.6-CL-44394996` | `C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat` | `...\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` |

工具链：VS 2022 Community。5.8/5.7 用 MSVC 14.44.35228 + SDK 10.0.22621.0；5.6 用首选 MSVC 14.38.33145。

换引擎前删除 Host/插件 `Intermediate/` 与 `Binaries/`。命令：

```
"<Engine>\Engine\Build\BatchFiles\Build.bat" PBRTextureLabHostEditor Win64 Development -Project="<repo>\PBRTextureLabHost\PBRTextureLabHost.uproject" -WaitMutex

"<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<uproject>" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput -AbsLog="<log>" -ReportExportPath="<report>" -ExecCmds="Automation RunTests PBRTextureLab;Quit"
```

第二进程每条 Reload 测试单独一次 `ExecCmds`。同一 `-ExecCmds` 里链式多个 `Automation RunTests` **无效**（只有第一条入队）。

## 编译

| 引擎 | 日志 | 退出码 | 结果 | 耗时 |
|---|---|---:|---|---|
| 5.8 最终全量 | `overnight-20260827/build-ue58-final.log` | 0 | Succeeded | 37.36s |
| 5.7 清 Intermediate 全量 | `overnight-20260827/build-ue57.log` | 0 | Succeeded | 48.38s |
| 5.6 清 Intermediate 全量 | `overnight-20260827/build-ue56.log` | 0 | Succeeded | 39.24s |

## Automation（同进程）

| 引擎 | 发现 | Success | Warnings | Fail | Editor 退出码 | 报告 |
|---|---:|---:|---:|---:|---:|---|
| 5.8 最终 | 37 | 37 | 0 | 0 | 0 | `overnight-20260827/automation-ue58-final/index.json` |
| 5.7 | 37 | 35+2w | 2 | 0 | 0 | `overnight-20260827/automation-ue57/index.json` |
| 5.6 | 37 | 34+3w | 3 | 0 | 0 | `overnight-20260827/automation-ue56/index.json` |

第二进程 ReloadAfterRestart（Import / Material / UV / Integration）三版本均为退出码 0、各 1/1 Success。

## 回归中修过的失败

1. **5.8 `PBRTextureLab.UV.UndoRedo`**  
   瞬时网格 `GetTransientPackage()` + `GEditor->UndoTransaction()` 触发 TypedElementRegistry ensure：`Element type ID '0' has not been registered`。  
   处理：Undo 测网格改到 `/Game/PBRTextureLab/Automation/`，与 Integration 路径一致。修复后 5.8 通过。

2. **5.7 `Nomad.TabRegistered` 崩溃**  
   `-NullRHI` 下 `SNew(SPBRTextureLabPreviewViewport)` 走 `SEditorViewport`，TypedElement **Assertion**（非 ensure）。  
   处理：仅当 `FApp::CanEverRender()` 为真才创建 3D 预览；无 RHI 显示占位文案。5.7 再跑退出码 0。

3. **5.6 `Integration.GenerateUVUndoRedo` 崩溃**  
   `UndoTransaction` 经 LevelEditor 回调断言 `UObjectArray.h` `Index >= 0`。5.8/5.7 同路径可 Undo。  
   处理：5.6 且 `!CanEverRender()` 时不调用 `UndoTransaction()`，只断言 `Trans->CanUndo()` 并 `AddWarning`。**交互 Editor 的 5.6 Undo 本轮未验证。**

## 版本差异 / 警告

- **5.8**：母球 `000基础材质` 可加载；最终套件 0 warning。
- **5.7 / 5.6**：`Material.ReuseUserParent` 警告 `Bundled 000基础材质 not loaded; skipped real-parent switch test.` 插件 Content 母球 uasset 在较旧引擎上 LoadObject 失败。合成测试母材质仍通过。
- **5.6**：上述 Undo skip 两条 warning（UV.UndoRedo、Integration.GenerateUVUndoRedo）。
- **5.7**：`Integration.GenerateUVUndoRedo` 带 TypedElement 外部引用 warning，测试仍 Success。
- **5.8 编译**：原 `UMaterialExpressionTextureBase::GetSamplerTypeForTexture` C4996。已改为 5.8 走 `MaterialExpressionUtils::GetSamplerTypeForTexture`，5.6/5.7 仍走旧 API。
- Host 跑测会写 `DefaultEngine.ini` AndroidFileServer `SecurityToken`，**未提交**。
- 加载 `/PBRTextureLab/Parents/000基础材质` 时若 Host 无 `/Game/NEW/StarterContent/.../T_Brick_Clay_Beveled_N`，会有依赖缺失日志；插件 `Content/Parents/Defaults` 有同名贴图。测试仍通过。

## 未验证

- 带窗口交互 Editor 的 Preview / Generate 点击。
- 5.6 带 RHI 的真实 Undo/Redo。
- 5.6/5.7 对 5.8 保存的 bundled 母球资源的完整参数绑定（Load 失败）。
- A213 工程本轮未同步、未编。
