# 1. 项目基本设置
cmake_minimum_required(VERSION 3.10)
project(MyAstroProgram C CXX)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 使用 PkgConfig 查找必需的外部依赖库
find_package(PkgConfig REQUIRED)
pkg_check_modules(CFITSIO REQUIRED cfitsio)
pkg_check_modules(GSL REQUIRED gsl)
pkg_check_modules(WCSLIB REQUIRED wcslib)
find_package(Threads REQUIRED)

# 4. 收集 astrometry.net 的源文件 (.c)
file(GLOB_RECURSE an_util_srcs "libs/astrometry/util/*.c")
file(GLOB_RECURSE an_blind_srcs "libs/astrometry/blind/*.c")
file(GLOB_RECURSE an_libkd_srcs "libs/astrometry/libkd/*.c")
file(GLOB_RECURSE an_qfits_srcs "libs/astrometry/qfits-an/*.c")

# 4.5. 从源文件列表中移除不应独立编译的模板文件
list(REMOVE_ITEM an_util_srcs
    # 核心实现模板
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/bl-nl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/dl-nl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/il-nl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/pl-nl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/sl-nl.c"
    # 排序模板
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/bl-nl-sort.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/dl-nl-sort.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/il-nl-sort.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/pl-nl-sort.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util/sl-nl-sort.c"
)
list(REMOVE_ITEM an_libkd_srcs
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/libkd/kd-nl-sort.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/libkd/kdtree_internal.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/libkd/kdtree_internal_dists.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/libkd/kdtree_internal_dualtree.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/libkd/kdtree_internal_fits.c"
)

# 5. 收集 sep 的源文件 (.cpp)
file(GLOB_RECURSE sep_srcs "libs/sep/*.cpp")

# 5.5. 收集工具源文件 (.cpp)
file(GLOB_RECURSE utils_srcs "libs/utils/*.cpp")

# 6. 收集程序源文件
# set(MY_APP_SRCS
#     demo.cpp
# )

set(API_SRCS
    astroapi.cpp
)

# 7. 创建可执行程序
# add_executable(my_astro_program
#     ${MY_APP_SRCS}
#     ${an_util_srcs}
#     ${an_blind_srcs}
#     ${an_libkd_srcs}
#     ${an_qfits_srcs}
#     ${sep_srcs}
#     ${utils_srcs}
# )

add_library(astroapi SHARED
    ${API_SRCS}
    ${an_util_srcs}
    ${an_blind_srcs}
    ${an_libkd_srcs}
    ${an_qfits_srcs}
    ${sep_srcs}
    ${utils_srcs}
)

# 7.5. 设置共享库属性
set_target_properties(astroapi PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 8. 将头文件包含路径精确地绑定到目标
target_include_directories(astroapi PUBLIC
    ${CFITSIO_INCLUDE_DIRS}
    ${GSL_INCLUDE_DIRS}
    ${WCSLIB_INCLUDE_DIRS}
    
    # 公共头文件路径
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/include/astrometry

    # 内部模块头文件路径
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/util
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/libkd
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/qfits-an
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/astrometry/blind

    # SEP 的头文件路径
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/sep

    ${CMAKE_CURRENT_SOURCE_DIR}/libs/utils
)

# 9. 链接所有需要的库
target_link_libraries(astroapi PUBLIC
    ${CFITSIO_LIBRARIES}
    ${GSL_LIBRARIES}
    ${WCSLIB_LIBRARIES}
    Threads::Threads
)