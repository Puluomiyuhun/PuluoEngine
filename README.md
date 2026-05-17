# PuluoEngine

一个从零开始构建的迷你 3D 游戏引擎，基于 C++20 和 OpenGL 4.5。

## 特性（规划中）

- **渲染系统** — OpenGL 4.5 Core 前向渲染管线，Phong 光照，模型加载
- **ECS 架构** — 自研 Entity-Component-System，高效管理游戏对象
- **场景系统** — 场景图，父子层级变换传播，场景切换
- **物理系统** — AABB / 球体碰撞检测与响应
- **音频系统** — 基于 miniaudio 的音效播放
- **脚本系统** — Lua 脚本绑定，支持热重载
- **编辑器** — ImGui 可视化编辑器（Viewport / Hierarchy / Inspector）
- **序列化** — YAML 场景存取

## 架构概览

```
PuluoEngine/
├── engine/
│   ├── core/          # 日志、数学、事件、Application 基类
│   ├── platform/      # GLFW 窗口管理 + 输入处理
│   ├── renderer/      # OpenGL 渲染封装
│   ├── ecs/           # Entity-Component-System
│   ├── scene/         # 场景图 + 场景管理
│   ├── resource/      # 资源加载与缓存
│   ├── physics/       # 碰撞检测
│   ├── audio/         # 音频引擎
│   ├── scripting/     # Lua 脚本
│   └── serialization/ # 序列化
├── editor/            # 编辑器应用
├── sandbox/           # 测试/演示应用
├── assets/            # 运行时资产
└── tests/             # 单元测试
```

## 技术栈

| 类别 | 选择 |
|------|------|
| 语言 | C++20 |
| 构建 | CMake 3.20+ |
| 图形 | OpenGL 4.5 Core |
| 窗口 | GLFW |
| 数学 | glm |
| 日志 | spdlog |
| 模型 | Assimp |
| GUI | Dear ImGui (docking) |
| 音频 | miniaudio |
| 脚本 | Lua 5.4 + sol2 |
| 序列化 | yaml-cpp |

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

## 开发路线

- [x] **阶段 0** — 项目骨架：CMake + 窗口 + 日志
- [ ] **阶段 1** — 渲染基础：Buffer / Shader / Texture / Camera
- [ ] **阶段 2** — 事件系统 + 输入抽象
- [ ] **阶段 3** — ECS + 场景系统
- [ ] **阶段 4** — 资源管理 + 模型加载
- [ ] **阶段 5** — 光照系统 (Phong)
- [ ] **阶段 6** — 物理系统
- [ ] **阶段 7** — 音频系统
- [ ] **阶段 8** — 序列化
- [ ] **阶段 9** — Lua 脚本
- [ ] **阶段 10** — 编辑器

## License

MIT
