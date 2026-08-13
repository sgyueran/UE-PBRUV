# Task 1 — UE API 与编译基线

## 目标

创建最小 C++ Host Project 和 `PBRTextureLab` Editor-only 插件骨架，并对 UE 5.6、5.7、5.8 建立真实 API/编译基线。

## 实施

- 用目标版本的真实头文件核验 Static Mesh MeshDescription/UV、AssetTools 导入、Material Instance 编辑、ToolMenus/TCommands/Selection 接口。
- 将版本差异集中在兼容层。
- 用每个版本的 `Build.bat` 构建 Development Editor；不要直接调用可能缺少运行时的 UBT 可执行文件。
- 不实现 PBR 生成、材质资产、UI 或 UV 修改。

## 验收

- 三版本各有构建命令、退出码和结果。
- API 清单包含签名、模块、头文件和行号。
- 任何引擎级第三方插件导致的 RulesError 都要独立记录，不得误归因于本插件。

