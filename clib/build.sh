# --- 配置 ---
# 构建目录的名称
BUILD_DIR="build"

# --- 脚本开始 ---

# 设置脚本在遇到错误时立即退出
set -e

# 1. 清理旧的构建目录
# 检查构建目录是否存在，如果存在则删除
if [ -d "$BUILD_DIR" ]; then
    echo "--- Cleaning previous build directory: $BUILD_DIR ---"
    rm -rf "$BUILD_DIR"
fi

# 2. 创建新的构建目录并进入
echo "--- Creating build directory: $BUILD_DIR ---"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

# 3. 运行 CMake 来配置项目
# ".." 指向上一级目录，即项目根目录，CMakeLists.txt 就在那里
echo "--- Running CMake... ---"
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 4. 运行 make 来编译项目
# -j 标志会尝试使用所有可用的 CPU核心来并行编译，以加快速度
echo "--- Building project... ---"
make -j$(nproc)

# 5. 完成
echo "--- Build finished successfully! ---"
echo "You can find the library at: $BUILD_DIR/libastroapi.so"

# 返回到项目根目录
cd ..