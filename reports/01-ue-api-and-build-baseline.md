# Task 1 Report — UE API 与编译基线

## 结论

Task 1 通过。已创建最小 C++ Host `PBRTextureLabHost` 和 Editor-only 插件 `PBRTextureLab`，并用本机 `Build.bat` 在 UE 5.6 / 5.7 / 5.8 完成 Development Editor 编译。未实现 PBR 生成、材质资产、UI 或 UV 修改。

## 前置依赖

无。

## 修改文件

- `PBRTextureLabHost/PBRTextureLabHost.uproject`
- `PBRTextureLabHost/Config/DefaultEngine.ini`
- `PBRTextureLabHost/Config/DefaultGame.ini`
- `PBRTextureLabHost/Config/DefaultEditor.ini`
- `PBRTextureLabHost/Source/PBRTextureLabHost.Target.cs`
- `PBRTextureLabHost/Source/PBRTextureLabHostEditor.Target.cs`
- `PBRTextureLabHost/Source/PBRTextureLabHost/*`
- `PBRTextureLabHost/Plugins/PBRTextureLab/PBRTextureLab.uplugin`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/PBRTextureLabEditor.Build.cs`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabCompat.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Public/PBRTextureLabEditorModule.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabEditorApi.h`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabApiProbe.cpp`
- `PBRTextureLabHost/Plugins/PBRTextureLab/Source/PBRTextureLabEditor/Private/PBRTextureLabEditorModule.cpp`
- `.gitignore`
- `reports/checkpoint.json`

## 验证命令与结果

工作目录：`C:\Users\Administrator\Desktop\Plughins`

工程：`PBRTextureLabHost\PBRTextureLabHost.uproject`

工具链：Visual Studio 2022 Community，MSVC 14.44.35228，Windows SDK 10.0.22621.0

| 引擎 | Build.bat | 退出码 | 结果 | 耗时 |
|---|---|---:|---|---|
| UE 5.6 | `C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 37.60s |
| UE 5.7 | `C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 37.20s |
| UE 5.8 | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` | 0 | Succeeded | 34.36s |

完整命令（三版本相同，仅替换引擎目录）：

```
"<Engine>\Engine\Build\BatchFiles\Build.bat" PBRTextureLabHostEditor Win64 Development -Project="C:\Users\Administrator\Desktop\Plughins\PBRTextureLabHost\PBRTextureLabHost.uproject" -WaitMutex
```

日志：

- `reports/build-ue56.log`
- `reports/build-ue57.log`
- `reports/build-ue58.log`

产物：`UnrealEditor-PBRTextureLabHost.dll`、`UnrealEditor-PBRTextureLabEditor.dll`

Editor 启动加载、PIE、UV 操作：**未运行，未验证**。

## 引擎级第三方插件（独立记录，非本插件错误）

| 引擎 | 插件 | 现象 | 处理 |
|---|---|---|---|
| UE 5.7（首次） | `Engine/Plugins/Connecter`（Design Connected，`EnabledByDefault: true`） | `RulesError`：`Expecting to find a type to be declared in a module rules named 'ConnecterUEPlugin' in 'UE5Rules'`，退出码 8 | 在 `.uproject` 中显式 `Enabled: false` 后重编通过 |
| UE 5.6 | 同插件，`EnabledByDefault: true` | 未单独复现首次失败；已一并禁用，避免污染基线 | 同上 |
| UE 5.8 | 同插件，`EnabledByDefault: false` | 未触发 | 无需处理 |

该插件不属于 Epic 发行物，不得归因于 `PBRTextureLab`。

## 兼容层

`PBRTextureLabCompat.h` + `PBRTextureLabEditorApi.h` 集中版本差异：

- `DefaultBuildSettings = Latest` / `IncludeOrderVersion = Latest`：5.6=V5，5.7=V6，5.8=V7。固定 V5 会在 5.8 触发 shared UnrealEditor 环境冲突（`UndefinedIdentifierWarningLevel` 等）。
- 5.8 新增 MIC：`SetDoubleVectorParameterValueEditorOnly`、`SetParameterCollectionParameterValueEditorOnly`。5.6/5.7 无此声明，用 `PBRTEXTURELAB_HAS_MIC_*` 守卫。
- 其余 Task 1 核验 API 在三版本签名一致，仅行号不同。

`PBRTextureLabApiProbe.cpp` 对核验过的成员函数取址，保证缺头文件或缺符号时编译失败。该文件不在 `StartupModule` 中调用。

## API 清单（本机头文件）

路径均相对于对应引擎的 `Engine/Source/`。

| API | 模块 | 头文件 | 签名 | 5.6 | 5.7 | 5.8 |
|---|---|---|---|---:|---:|---:|
| `UStaticMesh::GetMeshDescription` | Engine | `Runtime/Engine/Classes/Engine/StaticMesh.h` | `FMeshDescription* GetMeshDescription(int32 LodIndex) const` | 1445 | 1608 | 1627 |
| `UStaticMesh::CommitMeshDescription` | Engine | 同上 | `void CommitMeshDescription(int32 LodIndex, const FCommitMeshDescriptionParams& = {})` | 1485 | 1648 | 1667 |
| `UStaticMesh::GetNumSourceModels` | Engine | 同上 | `int32 GetNumSourceModels() const` | 1734 | 1949 | 1968 |
| `UStaticMesh::GetLightMapCoordinateIndex` | Engine | 同上 | `int32 GetLightMapCoordinateIndex() const` | — | — | 1207 |
| `FStaticMeshAttributes::GetVertexInstanceUVs` | StaticMeshDescription | `Runtime/StaticMeshDescription/Public/StaticMeshAttributes.h` | `TVertexInstanceAttributesRef<FVector2f> GetVertexInstanceUVs()` | 108 | 88 | 89 |
| `IAssetTools::ImportAssetTasks` | AssetTools | `Developer/AssetTools/Public/IAssetTools.h` | `virtual void ImportAssetTasks(const TArray<UAssetImportTask*>&)` | 479 | 539 | 542 |
| `FAssetToolsModule::Get` / `GetModule` | AssetTools | `Developer/AssetTools/Public/AssetToolsModule.h` | `IAssetTools& Get() const`；`static FAssetToolsModule& GetModule()` | 18/26 | 18/26 | 18/26 |
| `UAssetImportTask` | UnrealEd | `Editor/UnrealEd/Public/AssetImportTask.h` | `class UAssetImportTask : public UObject`；`GetObjects()` / `IsAsyncImportComplete()` | 25 | 25 | 25 |
| `UMaterialInstanceConstant::SetTextureParameterValueEditorOnly` | Engine | `Runtime/Engine/Public/Materials/MaterialInstanceConstant.h` | `void SetTextureParameterValueEditorOnly(const FMaterialParameterInfo&, UTexture*)` | 98 | 99 | 100 |
| `UMaterialInstanceConstant::SetScalarParameterValueEditorOnly` | Engine | 同上 | `void SetScalarParameterValueEditorOnly(const FMaterialParameterInfo&, float)` | 96 | 97 | 98 |
| `UMaterialInstanceConstant::SetDoubleVectorParameterValueEditorOnly` | Engine | 同上 | 仅 5.8 | 无 | 无 | 97 |
| `UMaterialInstanceConstant::SetParameterCollectionParameterValueEditorOnly` | Engine | 同上 | 仅 5.8 | 无 | 无 | 102 |
| `TCommands` | Slate | `Runtime/Slate/Public/Framework/Commands/Commands.h` | `TCommands(FName, const FText&, FName, FName)` | 110 | 110 | 110 |
| `UToolMenus::Get` | ToolMenus | `Developer/ToolMenus/Public/ToolMenus.h` | `static UToolMenus* Get()` | 109 | 109 | 109 |
| `UToolMenus::ExtendMenu` | ToolMenus | 同上 | `UToolMenu* ExtendMenu(const FName)` | — | — | 169 |
| `USelection` | UnrealEd | `Editor/UnrealEd/Public/Selection.h` | `class USelection : public UObject`；`GetSelectedObject(int32)` | 39 | 39 | 39 |
| `UEditorEngine::GetSelectedActors` | UnrealEd | `Editor/UnrealEd/Classes/Editor/EditorEngine.h` | `USelection* GetSelectedActors() const` | 1933 | 1935 | 1969 |
| `UEditorEngine::GetSelectedComponents` | UnrealEd | 同上 | `USelection* GetSelectedComponents() const` | — | 1966 | 2000 |
| `UEditorEngine::GetContentBrowserSelections` | UnrealEd | 同上 | `void GetContentBrowserSelections(TArray<FAssetData>&) const` | 1942 | 1944 | 1978 |
| `FScopedTransaction` | UnrealEd | `Editor/UnrealEd/Public/ScopedTransaction.h` | `FScopedTransaction(const FText&, bool = true)` | 11 | 11 | 11 |

`IContentBrowserSingleton::GetSelectedAssets` 位于 `Editor/ContentBrowser/Public/IContentBrowserSingleton.h:779`（5.8）。Task 1 选择 `GetContentBrowserSelections` 作为选择入口，避免额外拉入 CollectionManager 依赖。后续 Task 6 可再接入 Content Browser 菜单。

## 失败项

无未解决的本插件编译失败。

首次 5.8 使用 `BuildSettingsVersion.V5` 失败（退出码 6，shared environment mismatch）。已改为 `Latest`，属兼容层修正，不是引擎第三方插件问题。

## 已知限制

- 5.6 警告：本机 MSVC 14.44 不是 UE 5.6 首选 14.38。编译已通过，未换工具链。
- 三版本顺序编译会覆盖同一 `Intermediate/` / `Binaries/`。当前磁盘上的 DLL 属于最后一次成功的 UE 5.6 构建。
- 未启动 Editor，未验证模块热加载。
- 插件仅 Editor 模块；Game target 未单独编译（任务要求 Development Editor）。
- Metallic 不是可靠物理反演：本 Task 未实现，留给 Task 2 日志/UI。

## 下一任务

Task 2 离线 PBR 像素核心；Task 5 可与 Task 2 在 Task 1 通过后并行。
