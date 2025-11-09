#ifndef ASTRO_API_H
#define ASTRO_API_H

#include <stddef.h> // for size_t

#ifdef __cplusplus
extern "C"
{
#endif

    // This struct contains information about the astrometric solution
    // for an image.
    typedef struct SolutionForGo
    {
        double fieldWidth;  // The calculated width of the field in arcminutes
        double fieldHeight; // The calculated height of the field in arcminutes
        double ra;          // The Right Ascension of the center of the field in degrees
        double dec;         // The Declination of the center of the field in degrees
        double orientation; // The orientation angle of the image from North in degrees
        double pixscale;    // The pixel scale of the image in arcseconds per pixel
        double raError;     // The error between the search_ra position and the solution ra position in arcseconds
        double decError;    // The error between the search_dec position and the solution dec position in arcseconds
    } SolutionForGo;

    /**
     * @brief 从内存中的图像数据进行天体解析。
     *
     * 这个函数是线程安全的。
     *
     * @param image_data 指向包含完整图像文件内容（如 JPG, PNG）的内存缓冲区的指针。
     * @param image_data_size 图像数据缓冲区的大小（以字节为单位）。
     * @param index_path 指向 astrometry.net 索引文件所在目录的路径。
     * @param focal_length 相机的焦距（毫米）。如果为 0，将使用更广泛的搜索范围。
     * @param out_solution 指向一个 Solution 结构体的指针，用于接收解析结果。
     * @return int 状态码。0 表示成功，负值表示失败。
     *         -1: 无效的输入参数 (如空指针)。
     *         -2: 无法从内存解码图像。
     *         -3: 提取星点失败。
     *         -4: 天体解析失败。
     */
    int solve_from_memory(
        const unsigned char *image_data,
        size_t image_data_size,
        const char *index_path,
        double focal_length,
        SolutionForGo *out_solution);

#ifdef __cplusplus
}
#endif

#endif // ASTRO_API_H