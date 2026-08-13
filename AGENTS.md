# PBRTextureLab 工作约定

## 已完成

- **Task 1 通过**（2026-08-13）：最小 C++ Host `PBRTextureLabHost` + Editor-only 插件 `PBRTextureLab` 骨架、兼容层、API 编译探针。未实现 PBR / 材质 / UI / UV。
- **Task 2 通过**（2026-08-13）：离线、无 UObject 的 Metallic/Roughness 像素核心 + Automation Tests（纯色 / 水平渐变 / 垂直渐变 / 棋盘格 / 确定性 / Metallic / 非法输入）。三版本 Development Editor 编译退出码 0，各版本 `UnrealEditor-Cmd` 跑测 6/6 Success。报告见 `reports/02-offline-pbr-pixel-core.md`。未创建纹理资产、材质或 UI。
- **Task 3 通过**（2026-08-13）：ImageWrapper PNG + `UAssetImportTask`/`ImportAssetTasks` 导入 BaseColor/Normal/ORM/灰度贴图；处理取消、冲突、失败清理。三版本编译退出码 0；Automation 11/11；各版本第二次 Editor 进程 `ReloadAfterRestart` 1/1。报告见 `reports/03-texture-asset-import.md`。未创建材质实例。
- **Task 4 通过**（2026-08-13）：插件内容参数化 Metallic/Roughness 母材质 + `UMaterialInstanceConstant`；ORM 连线、可关闭 BumpOffset。三版本编译退出码 0；Automation 15/15；各版本第二次 Editor 进程 `Material.ReloadAfterRestart` 1/1。报告见 `reports/04-material-instance-generation.md`。未做 UV / UI。
- **Task 5 通过**（2026-08-13）：Static Mesh UV0 绝对 `100×`/`500×`；`UAssetUserData` 记录基准与指纹；Undo/Redo；外部 UV/拓扑变更要求重设基准。三版本编译退出码 0；Automation 20/20；各版本第二次 Editor 进程 `UV.ReloadAfterRestart` 1/1。报告见 `reports/05-uv-absolute-presets.md`。未做菜单/选择。
- **Task 6 通过**（2026-08-13）：`PBRTextureLab.UVScale100` / `UVScale500` 无默认快捷键；Tools 菜单、工具栏、Content Browser 右键；复制重绑 / 改源资产 / 取消。三版本编译退出码 0；Automation 24/24。报告见 `reports/06-editor-commands-and-selection.md`。未做 Nomad Tab。
- **Task 7 通过**（2026-08-13）：Nomad Tab（Texture2D / 本地图、输出目录尺寸参数、预览、生成）、接入 UV 命令；三版本编译退出码 0；Automation 30/30；各版本第二进程 `Integration.ReloadAfterRestart` 1/1。独立审查 Critical/High 为 0。报告见 `reports/07-integration-validation-review.md`。
- **三版本 Development Editor 编译通过**：UE 5.6 / 5.7 / 5.8，均用各引擎本机 `Build.bat`，退出码 0。报告见 `reports/01-ue-api-and-build-baseline.md` 至 `reports/07-integration-validation-review.md`。
- **Git**：仓库 `https://github.com/sgyueran/UE-PBRUV`，分支 `develop`，Task 7 提交 `a1a2a96`，Task 7 之后当前提交 `69ecca1`。约定每个 Task 完成后单独提交并推送。

## Task 7 之后（2026-08-13 ~ 2026-08-14）

自测工程：`C:\Users\Administrator\Documents\Unreal Projects\A213`（UE 5.8）。源码仍以本仓库 `PBRTextureLabHost/Plugins/PBRTextureLab` 为准，改完后同步进 A213 并清插件 `Binaries/Intermediate` 再编。不要改用户 `D:\UE5.8\Content\材质\MAT` 原件。

### 产品行为（当前）

- **母球**：插件目录 `/PBRTextureLab/Parents` 内置 13 个用户母球（`000基础材质` 等）。界面下拉选择；默认优先 `000基础材质`。按母球自定义参数**标题**驱动生成器文案（1:1）。
- **子材质可编辑**：不覆盖 `基础色` / `粗糙度` / `高光度` / `金属度`。插件回退母材质图里这四项乘到对应贴图，`法线强度` 默认 `0.1`。
- **贴图开关**：生成器取消勾选某通道 → 子材质只关该通道 `*贴图开关`（及同通道静态开关）。不写 `类型切换` / `反向`。未勾选不改法线/粗糙/金属强度；置换没有开关时把 `置换强度` 写成 0。
- **无 ORM**：用户管线不再生成/导入/绑定 ORM。
- **无缝**：导入/生成时边缘融合，采样 Wrap。
- **UV100 / UV500**：`NewUV = BaselineUV * (100,100)` / `(500,500)`。默认只改 LOD0，可选同步其他 LOD。菜单 `Set UV Tiling 100x100` / `500x500`，默认快捷键 Ctrl+Alt+1 / Ctrl+Alt+5（可重绑）。基准存在 `UPBRTextureLabUVPresetData`，100↔500 不累乘。
- **界面**：左右分栏——左参数/UV，右 2D 贴图预览 + 3D 预览。3D 可切 **球体 / 立方体 / 平面**（UE ThumbnailManager `EditorSphere/Cube/Plane`）。2D 只显示已勾选通道。两页都有新建/修改；可跳转子材质目录、把最近材质赋给选中模型。
- **资产**：每材质独立文件夹；`UniqueName`；本地文件/文件夹组装现成贴图组。

### 关键实现

- `BindGeneratedTextures`：按母球参数名分类通道；`EnabledMaps` + 有贴图才绑定；`*贴图开关` 开/关；静态开关用 `SetStaticSwitchParameterValueEditorOnly`（不要循环调 `UMaterialEditingLibrary` 的 setter，会反复建 `MaterialEditorInstance` 冲掉刚写的值）。
- 生成：`ExportFlags` → `EnabledMaps`。组装：有贴图的通道才开开关。
- 预览：`SPBRTextureLabPreviewViewport`（`FPreviewScene` + `SEditorViewport`）。
- 默认母球：`FindPreferredUserParent` 先 `/PBRTextureLab/Parents/000基础材质`，再 `/Game/材质/MAT`。

### 验证（已跑）

- Host UE 5.8 `Build.bat` PBRTextureLabHostEditor：退出码 0。
- `PBRTextureLab.Material` Automation（Host 5.8，含开关关断 / `000基础材质`）：5/5，退出码 0。日志 `reports/test-ue58-switch-default-off.log`。
- A213Editor Win64 Development：退出码 0（同步 Source 后）。
- 5.6 / 5.7 本轮**未**重跑全套 Automation，标为未验证。

### 已知限制

- `000基础材质` 没有 `法线贴图开关`，法线靠静态开关 `法线`；Automation 用 Global GET 读不到该开关时不要当失败。
- 未勾选通道若要把父级默认“开”改成“关”，子材质上该开关仍会显示为已覆盖（左边勾、值为关）。这是 MIC 覆盖语义，不是又打开了。
- Live Coding 会挡住 UBT；改插件后需退出 Editor 再编。
- 不要提交 Host 自动写的 `DefaultEngine.ini` AndroidFileServer `SecurityToken`，也不要把 Host `.uproject` 的 `EngineAssociation` 改成本机临时版本。

### 不要提交

- `YXgame/`、`dist/`、Host `Content/PBRTextureLab` 生成物、零散 Automation HTML/log 备份。
- 用户 `D:\UE5.8` 工程与 A213 工程本体（只同步插件源码进 A213）。
- **Connecter RulesError**：根因是 Design Connected 插件被装进安装版引擎 `Engine/Plugins/Connecter` 且 5.6/5.7 `EnabledByDefault: true`，UBT 去只读预编译 `UE5Rules.dll` 里找不到 `ConnecterUEPlugin`。已在本工程 `.uproject` 禁用；已把 5.6/5.7 引擎描述改成 `EnabledByDefault: false`（与 5.8 一致）。真正启用需要把插件挪到 `Engine/Plugins/Marketplace/Connecter`。该搬家会改 Program Files，需在可写系统目录的模式下执行，见 `sessions/2026-08-13-init-task1.md`。

## 项目目标

在 Unreal Engine 5.6–5.8 中实现 Editor-only 插件 `PBRTextureLab`：离线单图生成 UE Metallic/Roughness 工作流贴图，并提供 Static Mesh UV0 的绝对 `100×`、`500×` 缩放命令。

## 执行顺序

严格按 `tasks/01` 至 `tasks/07` 顺序执行。`02` 与 `05` 可在 `01` 通过后并行；其余任务不得跨越依赖开始。每个任务必须单独验证、单独报告。

## 防止 API 幻觉

- 网络文档只能用于定位；函数签名、头文件、模块依赖必须以目标 UE 版本的本机引擎源码和实际编译为准。
- 不得凭记忆调用 Unreal API；先检索真实头文件，再写代码。
- 每个构建报告必须列出 UE 版本、完整 Build.bat 命令、退出码和相关错误/警告。
- 未编译或未运行的行为必须标为“未验证”，不得写成通过。

## 范围与技术约束

- 插件仅含 Editor 模块，不能被运行时游戏模块依赖。
- 首版使用 UE 常规 Metallic/Roughness：BaseColor、Normal、Roughness、Metallic、AO、Height、ORM；这不是完整 OpenPBR 实现。
- 不接入在线 AI、CHORD、Materialize、UVEditor Beta 或未经许可核验的第三方代码。
- 像素计算可在后台线程运行；所有 UObject、资产导入、材质修改和 Editor 操作必须回到 Game Thread。
- 不修改 Skeletal Mesh、Landscape、Geometry Collection、运行时网格或 Lightmap UV。
- 所有资产/UV 改动必须使用 UE transaction，支持 Undo/Redo；不得删除或覆盖用户资产而没有显式确认。

## 审查与交付

- 保留无关用户修改，不做清理式重构。
- 每个 Task 交付：修改文件、依赖的前置 Task、验证命令及结果、失败项、已知限制。
- Task 7 需要独立审查实际 diff、构建日志和测试结果；不接受“看起来合理”作为完成证据。

