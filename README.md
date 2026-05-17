# PuluoEngine

一个从零开始构建的迷你 3D 游戏引擎，基于 C++20 和 OpenGL 4.5。

## 已实现特性

### 渲染系统
- PBR 前向渲染管线（金属度/粗糙度工作流）
- IBL 全局光照（辐照度卷积、预滤波环境贴图、BRDF LUT）
- 4 级联阴影贴图（CSM）
- SSAO 环境光遮蔽（半分辨率 + 模糊）
- 屏幕空间反射（SSR）
- FXAA 抗锯齿
- 程序化大气散射（Rayleigh + Mie）+ 时间变化
- 体积云（3D Worley/Perlin 噪声 + 光线步进）
- 高度雾 + 方向散射
- 水面渲染（反射、岸边渐隐、波纹动画）

### 地形系统
- GPU 曲面细分（vert/tesc/tese/frag 四阶段管线）
- 三层 PBR 材质混合（高度 + 坡度阈值）
- 噪声生成 / 高度图导入
- CPU 端高度查询

### 粒子与天气
- 粒子发射器（发射率、生命周期、颜色/尺寸曲线、重力、阻力）
- 天气系统（雨/雪，30000 粒子实时模拟）
- Alpha / Additive 混合模式

### 实例化渲染
- InstancedMesh 批量绘制（per-instance VBO）
- 视锥剔除 + 距离剔除

### 资源管理
- TextureCache / ModelCache 内存级缓存（`weak_ptr`，自动去重）
- `.passet` 二进制预处理格式（首次加载自动生成，后续跳过 Assimp/stbi 解码）
- 时间戳失效检测，原始文件更新后自动重新生成缓存

### 编辑器
- ImGui Docking 布局（Viewport / Scene Hierarchy / Inspector / Toolbar / Asset Browser）
- ImGuizmo 变换操控（平移/旋转/缩放）
- 鼠标拾取（Entity ID Framebuffer）
- Undo/Redo 系统
- 原生文件对话框（导入/保存/加载）
- 场景序列化（`.pscene` JSON 格式）

## 架构概览

```
PuluoEngine/
├── engine/
│   ├── core/          # 日志、数学、事件、Application、Scene、CommandHistory
│   ├── platform/      # GLFW 窗口 + 输入 + ImGui 层
│   ├── renderer/      # OpenGL 渲染封装（PBR、地形、粒子、天气、阴影、后处理等）
│   └── resource/      # 资源缓存（TextureCache/ModelCache）+ .passet 导入/加载
├── sandbox/           # 编辑器 / 运行时应用
├── assets/
│   ├── shaders/       # 53 个 GLSL 着色器文件
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
- 所有第三方依赖通过 CMake FetchContent 自动拉取，无需手动安装

### 编译步骤

```bash
git clone https://github.com/Puluomiyuhun/PuluoEngine.git
cd PuluoEngine
cmake -B build
cmake --build build --config Release
```

### 运行

```bash
./build/sandbox/Release/PuluoSandbox   # Windows
# 或
./build/sandbox/PuluoSandbox            # Linux/macOS
```

> 注：模型和 HDR 环境贴图等大文件资产未包含在仓库中，首次运行需自行放入 `assets/models/` 和 `assets/textures/hdr/` 目录。

## 开发路线

- [x] 项目骨架：CMake + 窗口 + 日志
- [x] 渲染基础：Buffer / Shader / Texture / Camera
- [x] 事件系统 + 输入抽象
- [x] 场景系统 + 序列化
- [x] 资源管理 + 模型加载 + .passet 缓存
- [x] PBR 光照 + IBL
- [x] 地形（曲面细分 + 多层材质）
- [x] 阴影（CSM）+ SSAO + SSR + FXAA
- [x] 大气 / 体积云 / 雾
- [x] 粒子系统 + 天气系统
- [x] 实例化渲染 + 视锥剔除
- [x] ImGui 编辑器
- [ ] ECS 架构重构
- [ ] RenderGraph / RenderPass 抽象
- [ ] 物理系统
- [ ] 音频系统
- [ ] Lua 脚本

## License

MIT
