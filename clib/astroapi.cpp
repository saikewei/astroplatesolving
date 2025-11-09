#include "astroapi.h"
#include "image_loader.h"
#include "extractor_solver.h"
#include <vector>
#include <string>

// stb_image.h 需要在一个 .cpp 文件中定义 STB_IMAGE_IMPLEMENTATION
// 由于 image_loader.cpp 已经这样做了，我们这里只需要包含头文件即可。
#include "stb_image.h"
#include "sep.h" // For SEP_TBYTE

// C API 函数实现
int solve_from_memory(
    const unsigned char *image_data,
    size_t image_data_size,
    const char *index_path,
    double focal_length,
    SolutionForGo *out_solution)
{
    if (!image_data || image_data_size == 0 || !index_path || !out_solution)
    {
        return -1; // 无效参数
    }

    ImageStats stats;
    uint8_t *buffer = nullptr;

    // 1. 从内存加载图像
    int load_result = load_image_from_memory_to_planar_buffer(image_data, image_data_size, &stats, &buffer);
    if (load_result != 0)
    {
        return -2; // 图像解码失败
    }

    try
    {
        // 2. 创建解析器实例
        AstroExtractorSolver extractor(stats, buffer);

        // 3. 提取星点
        if (extractor.extract_stars() < 0)
        {
            free_image_buffer(buffer);
            return -3; // 提取星点失败
        }

        // 4. 进行天体解析
        if (extractor.solve(std::string(index_path), focal_length) != 0)
        {
            free_image_buffer(buffer);
            return -4; // 解析失败
        }

        // 5. 获取并复制结果
        Solution temp = extractor.getSolution();
        out_solution->fieldWidth = temp.fieldWidth;
        out_solution->fieldHeight = temp.fieldHeight;
        out_solution->ra = temp.ra;
        out_solution->dec = temp.dec;
        out_solution->orientation = temp.orientation;
        out_solution->pixscale = temp.pixscale;
        out_solution->raError = temp.raError;
        out_solution->decError = temp.decError;
    }
    catch (...)
    {
        // 捕获任何 C++ 异常，防止其传播到 C/Go 层
        free_image_buffer(buffer);
        return -99; // 未知 C++ 异常
    }

    // 6. 释放图像缓冲区内存
    free_image_buffer(buffer);

    return 0; // 成功
}