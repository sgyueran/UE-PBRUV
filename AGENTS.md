# PBRTextureLab 工作约定

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

