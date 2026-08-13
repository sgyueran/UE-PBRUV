# Task 2 — 离线 PBR 像素算法

## 依赖

Task 1 完成。

## 目标

实现纯 C++、不依赖 UObject 的单图 PBR 通道生成核心。

## 实施

- 输入 RGBA 图片；输出 BaseColor、Height、Normal、AO、Roughness、Metallic、ORM。
- Height 基于亮度，可调对比度、模糊和反转。
- Normal 用 Height 的 Sobel 梯度生成，方向适配 UE。
- AO 用确定性的多尺度高度邻域估算。
- Roughness 用局部亮度变化和偏置估算。
- Metallic 默认全黑，可选常量/阈值蒙版；UI/日志必须说明其不是可靠物理反演。
- ORM 固定：R=AO、G=Roughness、B=Metallic。
- 不创建 UE 资产、不联网、不引入第三方 AI 或模型。

## 验收

- Automation Tests 覆盖纯色、水平/垂直渐变、棋盘格。
- 验证尺寸、数值范围、ORM 通道和法线方向。
- 固定输入和参数必须产生确定性输出。

