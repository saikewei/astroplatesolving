# Astro Plate Solving API

由于本人用于天文摄影的设备过于简陋，时常在寻找目标的时候浪费很多时间，所以闲来无事做了本项目。

这是一个基于 Go 和 C++ 的天体解析（Plate Solving）Web 后端。它接收一张天文图像和相机焦距，并返回该图像在天空中的精确坐标（赤经/赤纬）、视场大小、旋转角度和像素比例尺。

该项目的核心是一个 C++ 库，它封装了 `astrometry.net` 引擎用于解析，以及 `sep` (Source Extractor) 用于快速准确的星点提取。注意，本项目使用的`astrometry.net` 和`sep` 是 `stellarsolver` 魔改之后的版本。Go 语言调用这个 C++ 库，并使用 Gin 框架提供了一个简单易用的 HTTP API。

- [StellarSolver](https://github.com/rlancaste/stellarsolver)
- [astrometry.net](https://astrometry.net/)
- [sep](https://github.com/kbarbary/sep) (Source Extractor)

## 主要技术栈

- **后端服务**: Go (Golang)
- **Web 框架**: Gin
- **核心算法库**: C++ (17)
- **天体解析**: `astrometry.net`
- **星点提取**: `sep` (Source Extractor C++ port)
- **图像解码**: `stb_image`
- **C++ 库构建**: CMake
- **Go/C 交互**: `cgo`

## 环境准备

在构建和运行此项目之前，请确保您的系统（推荐 Linux）已安装以下依赖：

1.  **Go**: 1.18 或更高版本。
2.  **C/C++ 构建工具**: `gcc`, `g++`, `make`, `cmake`。
3.  **Pkg-config**: 用于查找 C 库。
4.  **核心 C 库**: `cfitsio`, `gsl`, `wcslib`。
    在 Ubuntu/Debian 系统上，可以通过以下命令安装：
    ```bash
    sudo apt-get update
    sudo apt-get install build-essential cmake pkg-config libcfitsio-dev libgsl-dev wcslib-dev
    ```
5.  **Astrometry.net 索引文件**:
    - 从 [data.astrometry.net](http://data.astrometry.net/4200/) 下载索引文件（`.fits` 文件）。
    - 根据您图像的视场大小选择合适的索引系列。通常 `4207` 到 `4212` 系列可以覆盖大部分场景。
    - 将下载的 `.fits` 文件放置在一个目录中。

## 构建与运行

### 步骤 1: 编译 C++ 共享库

首先，需要编译 C++ 部分以生成 `libastroapi.so` 文件。

```bash
# 进入 clib 目录
cd clib

# 运行编译脚本
./build.sh
```

脚本会自动完成清理、配置和编译。成功后，您应该能在 `clib/build/` 目录下找到 `libastroapi.so` 文件。

### 步骤 2: 运行 Go Web 服务

回到项目根目录，运行 Go 服务。

```bash
# 回到项目根目录
cd ..

# 运行 Go 程序
go run main.go
```

服务将启动并监听 `8080` 端口。

## API 使用说明

### 端点: `POST /api/solve`

该端点用于提交图像并获取解析结果。

- **请求类型**: `multipart/form-data`
- **表单字段**:
  - `image`: 必需，图像文件 (如 `.jpg`, `.png` 等)。
  - `focal_length`: 必需，相机的焦距，单位为毫米 (mm)。

#### 示例请求 (使用 cURL)

```bash
curl -X POST http://localhost:8080/api/solve \
  -F "image=@/path/to/your/astro_image.jpg" \
  -F "focal_length=200"
```

#### 成功响应 (HTTP 200 OK)

如果解析成功，服务器将返回一个包含天体测量解的 JSON 对象。

```json
{
  "FieldWidth": 429.15,
  "FieldHeight": 286.1,
  "RA": 210.802,
  "Dec": 54.345,
  "Orientation": 90.5,
  "Pixscale": 4.29,
  "RAError": 1.2,
  "DecError": 0.8
}
```

- `RA`, `Dec`: 视场中心的赤经和赤纬 (度)。
- `FieldWidth`, `FieldHeight`: 视场的宽度和高度 (角分)。
- `Orientation`: 图像指北方向的旋转角度 (度)。
- `Pixscale`: 像素比例尺 (角秒/像素)。
- `RAError`, `DecError`: 解算结果与搜索位置的误差 (角秒)。

#### 失败响应 (HTTP 4xx/5xx)

如果发生错误，服务器将返回一个包含错误信息的 JSON 对象。

```json
{
  "error": "天体解析失败",
  "details": "astrometry solving failed with C code: -4"
}
```
