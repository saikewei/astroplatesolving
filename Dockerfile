# 使用官方 Go 镜像作为构建环境
FROM golang:1.21 AS builder

WORKDIR /app

ENV GOPROXY=https://goproxy.cn,direct

# 复制 Go 源码和 C/C++ 代码
COPY . .

# 构建 C++ 共享库
WORKDIR /app/clib
RUN apt-get update && \
    apt-get install -y build-essential cmake pkg-config libcfitsio-dev libgsl-dev wcslib-dev && \
    ./build.sh

# 回到 Go 项目根目录
WORKDIR /app

# 构建 Go 可执行文件（静态链接 .so）
RUN go build -o astro_server main.go

# ---- 运行环境 ----
FROM ubuntu:22.04

WORKDIR /app

# 安装运行所需的依赖
RUN apt-get update && \
    apt-get install -y libcfitsio9 libgsl27 wcslib-dev

# 复制可执行文件、C 库、配置文件
COPY --from=builder /app/astro_server .
COPY --from=builder /app/clib/build/libastroapi.so ./clib/build/
COPY config.yaml .
# 日志文件可由容器挂载卷生成，不需要提前 COPY

# 暴露端口
EXPOSE 24568

# 设置环境变量，确保 .so 能被找到
ENV LD_LIBRARY_PATH=/app/clib/build

CMD ["/bin/bash", "-c", "./astro_server > server.log 2>&1"]