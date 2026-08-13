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
- **Git**：仓库 `https://github.com/sgyueran/UE-PBRUV`，分支 `develop`，Task 7 提交 `a1a2a96`。约定每个 Task 完成后单独提交并推送。
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

