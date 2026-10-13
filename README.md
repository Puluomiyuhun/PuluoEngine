# PuluoEngine

一个从零开始构建的迷你 3D 游戏引擎，基于 C++20 和 OpenGL 4.5。

## 已实现特性

### 渲染系统
- PBR 前向渲染管线（金属度/粗糙度工作流）
- IBL 全局光照（辐照度卷积、预滤波环境贴图、BRDF LUT）
- 4 级联阴影贴图（CSM）+ 级联间平滑过渡
- SSAO 环境光遮蔽（半分辨率 + 模糊）
- 屏幕空间反射（SSR）
- FXAA 抗锯齿
- 程序化大气散射（Rayleigh + Mie）+ 内置方向光（太阳随大气高度自动同步）
- 体积云（3D Worley/Perlin 噪声 + 光线步进）
- 高度雾 + 方向散射
- 水面渲染（反射、岸边渐隐、波纹动画）
- RenderPass 抽象体系（ShadowPass / DepthPrepass / SSAOPass / PostProcessPass）
- Alpha Mask + SSS 次表面散射（植物材质）
- 模型视锥剔除 + 渲染统计（FPS / Draw Call / 剔除数）

### 地形系统
- GPU 曲面细分（vert/tesc/tese/frag 四阶段管线）
- 距离自适应 LOD（近处高细分，远处低细分）
- 三层 PBR 材质混合（高度 + 坡度阈值）
- IBL 间接光照（和模型统一的环境光）
- Patch 级视锥剔除
- 噪声生成 / 高度图导入
- CPU 端高度查询

### 粒子与天气
- 粒子发射器（发射率、生命周期、颜色/尺寸曲线、重力、阻力）
- 天气系统（雨/雪，30000 粒子实时模拟）
- Alpha / Additive 混合模式

### 实例化渲染
- InstancedMesh 批量绘制（per-instance VBO）
- 视锥剔除 + 距离剔除
- 地形贴地（Snap to Terrain）

### 资源管理
- TextureCache / ModelCache 内存级缓存（`weak_ptr`，自动去重）
- `.passet` 二进制预处理格式（模型自包含所有关联贴图）
- 三级加载策略：.passet 直读 → 磁盘缓存 + 时间戳校验 → Assimp/stbi 回退
- Import-to-Project 资产管线（外部文件导入为 .passet 项目资产）
- 独立贴图 .passet 支持

### 编辑器
- ImGui Docking 布局（Viewport / Scene Hierarchy / Inspector / Toolbar / Asset Browser）
- ImGuizmo 变换操控（平移/旋转/缩放）
- 鼠标拾取（Entity ID Framebuffer）
- Undo/Redo 系统
- Asset Browser（导入/删除/重命名 .passet 资产、批量导入）
- Material Override（场景级材质覆盖，不修改原始 .passet）
- 渲染统计 Overlay（F3 切换，平滑 FPS / Draw Call / 剔除数）
- 编辑器主题系统（Dark Slate / Pink Cute）
- 原生文件对话框 + ImGui 内置资产选择器
- 场景序列化（`.pscene` JSON 格式）

### 材质设计器（PuluoDesigner）

节点式程序化材质工具，独立 exe，单向依赖引擎（引擎无感知）。

- 节点图编辑器（手写 ImGui 画布）：拖拽连线、框选移动、滚轮光标锚定缩放、右键添加节点
- 每个节点带纹理缩略图预览（棋盘格底 + 角色色条）
- GPU 求值：compute shader 逐节点 dispatch，RGBA16F 纹理池复用，拓扑排序 + 结果记忆化
- Atomic 节点：Uniform Color / Levels / Blend（8 种混合模式）
- Levels 直方图编辑器：GPU 256-bin 直方图（log 刻度）、5 个可拖手柄、传递曲线叠加、Invert / Auto Level / 纯 slider 界面切换
- 输出节点：Base Color / Normal / Roughness / Metallic / AO 五通道，分辨率 64–4096（存 log2，图分辨率无关）
- 3D 预览：球体实时 PBR + IBL，与节点图同进程同 GL 上下文，**贴图零拷贝零落盘**直接喂给 pbr.frag
- 2D 预览：双击节点查看其输出贴图

## 架构概览

```
PuluoEngine/
├── engine/
│   ├── core/          # 日志、数学、事件、Application、Scene、CommandHistory
│   ├── platform/      # GLFW 窗口 + 输入 + ImGui 层
│   ├── renderer/      # OpenGL 渲染封装（PBR、地形、粒子、天气、RenderPass、阴影、后处理）
│   └── resource/      # 资源缓存（TextureCache/ModelCache）+ .passet 导入/加载
├── sandbox/           # 编辑器 / 运行时应用
├── designer/          # 材质设计器（独立 exe，单向依赖引擎）
│   └── src/{graph,gpu,ui,preview}
├── assets/
│   ├── shaders/       # GLSL 着色器文件（含 texgraph/ 下的 compute kernel）
│   └── fonts/         # UI 字体
└── cmake/             # 依赖管理（FetchContent）
```

## 技术栈

| 类别 | 选择 |
|------|------|
| 语言 | C++20 |
| 构建 | CMake 3.20+ |
| 图形 | OpenGL 4.5 Core |
| 窗口 | GLFW 3.4 |
| 数学 | glm 1.0 |
| 日志 | spdlog |
| 模型 | Assimp 5.4（OBJ / glTF / FBX） |
| GUI | Dear ImGui 1.91 (docking) + ImGuizmo |
| 序列化 | nlohmann/json |
| 文件对话框 | nativefiledialog-extended |

## 构建

### 环境要求

- CMake 3.20+
- 支持 C++20 的编译器（MSVC 2022 / GCC 12+ / Clang 15+）
- Git 和 Python 3（glad 的 OpenGL loader 在配置阶段由 Python 生成，需要 `jinja2`）
- 其余第三方依赖通过 CMake FetchContent 自动拉取，无需手动安装

### 编译步骤

```bash
git clone https://github.com/Puluomiyuhun/PuluoEngine.git
cd PuluoEngine
cmake -B build
cmake --build build --config Release
```

构建目标由选项控制：`PULUO_BUILD_SANDBOX`（默认 ON）、`PULUO_BUILD_DESIGNER`（默认 ON）。

### 运行

```bash
./build/sandbox/Release/PuluoSandbox     # 引擎编辑器
./build/designer/Release/PuluoDesigner   # 材质设计器
```

> 注：模型和 HDR 环境贴图等大文件资产未包含在仓库中，首次运行需自行放入 `assets/models/` 和 `assets/textures/hdr/` 目录。

## 开发路线

- [x] 项目骨架：CMake + 窗口 + 日志
- [x] 渲染基础：Buffer / Shader / Texture / Camera
- [x] 事件系统 + 输入抽象
- [x] 场景系统 + 序列化
- [x] 资源管理 + .passet 资产管线
- [x] PBR 光照 + IBL
- [x] 地形（曲面细分 + 多层材质 + IBL）
- [x] 阴影（CSM）+ SSAO + SSR + FXAA
- [x] 大气 / 体积云 / 雾 / 内置太阳光
- [x] 粒子系统 + 天气系统
- [x] 实例化渲染 + 视锥剔除
- [x] RenderPass 抽象
- [x] ImGui 编辑器 + Asset Browser + 主题
- [x] 渲染统计 + 性能优化
- [x] 材质节点系统（PuluoDesigner：节点图 + GPU 求值 + PBR 预览）
- [ ] 材质设计器：灰度/彩色类型系统
- [ ] 材质设计器：子图封装（库节点）
- [ ] 材质设计器：Pixel Processor / FX-Map
- [ ] 地形材质笔刷（Splat Map）
- [ ] ECS 架构重构
- [ ] RenderGraph
- [ ] 物理系统
- [ ] 音频系统
- [ ] Lua 脚本

## License

MIT
