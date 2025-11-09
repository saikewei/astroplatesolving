#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <stdint.h>
#include <stddef.h>

// 1. 定义与原 stats 结构体对应的 C 结构体
typedef struct
{
    int width;
    int height;
    int channels;        // 通常是 3 (R, G, B)
    int bytes_per_pixel; // 通常是 1 (对于 8-bit 图像)
    int ndim;
    uint32_t samples_per_channel;
    // SEP 需要的数据类型
    int sep_data_type;
} ImageStats;

// 2. 声明加载函数
/**
 * @brief 从文件加载图像，并转换为 StellarSolver 需要的平面格式。
 * @param filename 要加载的图像文件路径。
 * @param out_stats 函数成功时，会填充这个结构体。
 * @param out_buffer 函数成功时，会通过这个指针返回分配的图像缓冲区。
 *                   调用者必须在用完后调用 free_image_buffer() 来释放它。
 * @return 0 表示成功, 非 0 表示失败。
 */
int load_image_to_planar_buffer(const char *filename, ImageStats *out_stats, uint8_t **out_buffer);

/// @brief
/// @param image_data
/// @param image_data_size
/// @param out_stats
/// @param out_buffer
/// @return
int load_image_from_memory_to_planar_buffer(const unsigned char *image_data, size_t image_data_size, ImageStats *out_stats, uint8_t **out_buffer);

// 3. 声明内存释放函数
/**
 * @brief 释放由 load_image_to_planar_buffer 分配的内存。
 * @param buffer 要释放的缓冲区指针。
 */
void free_image_buffer(uint8_t *buffer);

#endif // IMAGE_LOADER_H