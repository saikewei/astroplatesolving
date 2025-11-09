#include "image_loader.h"
#include <stdio.h>
#include <stdlib.h> // for malloc and free

// 在一个实现文件中定义这个宏，然后包含 stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// 包含 SEP 的头文件以获取数据类型常量
#include "sep.h"

int load_image_to_planar_buffer(const char *filename, ImageStats *out_stats, uint8_t **out_buffer)
{
    if (!filename || !out_stats || !out_buffer)
    {
        return -1; // 无效参数
    }

    int width, height, channels_in_file;

    // 1. 使用 stb_image 解码图像
    // stbi_load 会自动检测文件格式并解码
    // 它返回一个指向交错存储 (RGBRGB...) 的像素数据缓冲区的指针
    unsigned char *interleaved_buffer = stbi_load(filename, &width, &height, &channels_in_file, 3);

    if (interleaved_buffer == NULL)
    {
        fprintf(stderr, "Error loading image '%s': %s\n", filename, stbi_failure_reason());
        return -2; // 加载失败
    }

    // 2. 填充 stats 结构体 (与原代码逻辑相同)
    out_stats->width = width;
    out_stats->height = height;
    out_stats->channels = 3;        // 我们强制加载为 3 通道
    out_stats->bytes_per_pixel = 1; // 8-bit per channel
    out_stats->sep_data_type = SEP_TBYTE;
    out_stats->ndim = 3;
    out_stats->samples_per_channel = out_stats->width * out_stats->height;

    // 3. 分配我们自己的目标缓冲区 (用于平面格式)
    size_t samples_per_channel = width * height;
    size_t total_size = samples_per_channel * out_stats->channels;

    uint8_t *planar_buffer = (uint8_t *)malloc(total_size);
    if (planar_buffer == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for planar buffer.\n");
        stbi_image_free(interleaved_buffer); // 别忘了释放 stb 加载的内存
        return -3;                           // 内存分配失败
    }

    // 4. 执行内存布局转换 (与原代码逻辑完全相同)
    // 从交错的 RGBRGB... 转换为平面的 RRR...GGG...BBB...
    uint8_t *r_plane = planar_buffer;
    uint8_t *g_plane = planar_buffer + samples_per_channel;
    uint8_t *b_plane = planar_buffer + (samples_per_channel * 2);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // stb_image 的输出是 RGB 顺序
            unsigned char *pixel = interleaved_buffer + (y * width + x) * 3;
            r_plane[y * width + x] = pixel[0]; // R
            g_plane[y * width + x] = pixel[1]; // G
            b_plane[y * width + x] = pixel[2]; // B
        }
    }

    // 5. 释放 stb_image 创建的临时缓冲区
    stbi_image_free(interleaved_buffer);

    // 6. 通过输出参数返回结果
    *out_buffer = planar_buffer;

    return 0; // 成功
}

int load_image_from_memory_to_planar_buffer(const unsigned char *image_data, size_t image_data_size, ImageStats *out_stats, uint8_t **out_buffer)
{
    if (!image_data || image_data_size == 0 || !out_stats || !out_buffer)
    {
        return -1; // 无效参数
    }

    int width, height, channels_in_file;
    // 从内存加载图像
    unsigned char *interleaved_buffer = stbi_load_from_memory(image_data, image_data_size, &width, &height, &channels_in_file, 3);

    if (interleaved_buffer == NULL)
    {
        // stbi_failure_reason() 可以提供更多信息
        return -2; // 加载失败
    }

    // 填充 stats 结构体
    out_stats->width = width;
    out_stats->height = height;
    out_stats->channels = 3;
    out_stats->bytes_per_pixel = 1;
    out_stats->sep_data_type = SEP_TBYTE;
    out_stats->ndim = 3;
    out_stats->samples_per_channel = out_stats->width * out_stats->height;

    // 分配平面格式的缓冲区
    size_t samples_per_channel = width * height;
    size_t total_size = samples_per_channel * out_stats->channels;
    uint8_t *planar_buffer = (uint8_t *)malloc(total_size);
    if (planar_buffer == NULL)
    {
        stbi_image_free(interleaved_buffer);
        return -3; // 内存分配失败
    }

    // 转换: 从交错 (RGBRGB...) 到平面 (RRR...GGG...BBB...)
    uint8_t *r_plane = planar_buffer;
    uint8_t *g_plane = planar_buffer + samples_per_channel;
    uint8_t *b_plane = planar_buffer + (samples_per_channel * 2);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            unsigned char *pixel = interleaved_buffer + (y * width + x) * 3;
            r_plane[y * width + x] = pixel[0];
            g_plane[y * width + x] = pixel[1];
            b_plane[y * width + x] = pixel[2];
        }
    }

    stbi_image_free(interleaved_buffer);
    *out_buffer = planar_buffer;
    return 0; // 成功
}

void free_image_buffer(uint8_t *buffer)
{
    if (buffer)
    {
        free(buffer);
    }
}