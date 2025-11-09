#include <iostream>
#include "image_loader.h"
#include "extractor_solver.h"

int main()
{
    // --- 测试 image_loader ---
    std::cout << "Testing image loader..." << std::endl;

    // !!重要!!: 请将 "path/to/your/image.jpg" 替换为一张真实图片的有效路径
    // 例如: "C:/Users/YourUser/Pictures/m31.jpg" 或 "./test_images/my_photo.png"
    const char *test_image_path = "/mnt/d/temp/SNY03438.jpg";

    ImageStats stats;
    uint8_t *buffer = nullptr;

    // 调用加载函数
    int result = load_image_to_planar_buffer(test_image_path, &stats, &buffer);

    // 检查结果
    if (result != 0)
    {
        std::cerr << "Failed to load image: " << test_image_path << std::endl;
        return -1;
    }

    AstroExtractorSolver extractor(stats, buffer);
    extractor.extract_stars();
    extractor.print_stars();
    extractor.solve("/home/saikewei/star_index", 200);

    free_image_buffer(buffer);
    return 0;
}