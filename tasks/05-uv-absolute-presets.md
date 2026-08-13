# Task 5 — UV 100×/500×绝对预设

## 依赖

Task 1 完成。

## 目标

实现 Static Mesh UV0 全部可编辑源 LOD 的绝对缩放预设。

## 实施

- 仅处理 Static Mesh 的 UV0 和全部可编辑源 LOD；不修改 Lightmap UV 或其他通道。
- 提供 `100×`、`500×` 绝对预设；`100→500→100` 不能累计放大。
- 用资产元数据记录当前比例及 UV/拓扑指纹。
- 检测外部 UV/拓扑变更；失配时要求重新设基准。
- 使用 `FScopedTransaction` 实现 Undo/Redo。
- 不使用 UVEditor Beta 或自动展开 API。

## 验收

- 各 LOD 的 UV0 数值正确。
- 预设切换无累计误差。
- Undo/Redo 恢复 UV 与元数据。
- 不支持对象安全失败且不崩溃。

