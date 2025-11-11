#include <vector>
#include <list>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <future>
#include <mutex>
#include <utility>
#include <iomanip>

#include "image_loader.h"
#include "extractor_solver.h"
#include "extract.h"

extern "C"
{
#include "astrometry/log.h"
#include "astrometry/sip-utils.h"
}

using namespace SEP;

AstroExtractorSolver::AstroExtractorSolver(const ImageStats &_imagestats, uint8_t const *_imageBuffer)
    : imagestats(_imagestats),
      imageBuffer(_imageBuffer),
      hasExtracted(false),
      job(&thejob),
      hasWCS(false),
      hasSolved(false),
      solutionIndexNumber(-1),
      solutionHealpix(-1)
{
}

int AstroExtractorSolver::extract_stars()
{
    std::cout << "开始提取恒星..." << std::endl;

    uint32_t x = 0, y = 0;
    uint32_t w = imagestats.width, h = imagestats.height;
    uint32_t raw_w = imagestats.width, raw_h = imagestats.height;

    std::vector<float *> dataBuffers;
    std::vector<StartupOffset> startupOffsets;
    std::vector<Background> backgrounds;

    int default_margin = MAX_SIZE / 2;
    if (default_margin <= 20)
        default_margin = 20;
    else if (default_margin > 50)
        default_margin = 50;

    if (w > PARTITION_SIZE && h > PARTITION_SIZE)
    {
        int W_PARTITION_SIZE = PARTITION_SIZE;
        int H_PARTITION_SIZE = PARTITION_SIZE;

        // Limit it to PARTITION_THREADS
        if ((w * h) / (W_PARTITION_SIZE * H_PARTITION_SIZE) > PARTITION_THREADS)
        {
            W_PARTITION_SIZE = w / (PARTITION_THREADS / 2);
            H_PARTITION_SIZE = h / 2;
        }

        int horizontalPartitions = w / W_PARTITION_SIZE;
        int verticalPartitions = h / H_PARTITION_SIZE;
        int horizontalOffset = w - (W_PARTITION_SIZE * horizontalPartitions);
        int verticalOffset = h - (H_PARTITION_SIZE * verticalPartitions);

        std::cout << "图像较大，使用分区处理模式 (" << horizontalPartitions << "x" << verticalPartitions
                  << " = " << horizontalPartitions * verticalPartitions << " 个分区)" << std::endl;

        // 预先分配 vector 的容量，防止内存重分配
        int totalPartitions = horizontalPartitions * verticalPartitions;
        backgrounds.resize(totalPartitions);

        int partitionIndex = 0;

        for (int i = 0; i < verticalPartitions; i++)
        {
            for (int j = 0; j < horizontalPartitions; j++)
            {
                int offsetW = (j == horizontalPartitions - 1) ? horizontalOffset : 0;
                int offsetH = (i == verticalPartitions - 1) ? verticalOffset : 0;

                const uint32_t rawStartX = x + j * W_PARTITION_SIZE;
                const uint32_t rawStartY = y + i * H_PARTITION_SIZE;
                const uint32_t rawEndX = rawStartX + W_PARTITION_SIZE + offsetW;
                const uint32_t rawEndY = rawStartY + H_PARTITION_SIZE + offsetH;

                uint32_t startX, startY, subWidth, subHeight;
                computeMargin(rawStartX, rawStartY, rawEndX - 1, rawEndY - 1,
                              imagestats.width, imagestats.height, default_margin,
                              &startX, &startY, &subWidth, &subHeight);

                startupOffsets.push_back(StartupOffset(startX, startY, subWidth, subHeight,
                                                       rawStartX, rawStartY, rawEndX - 1, rawEndY - 1));
                float *data = allocateFloatBuffer(startX, startY, subWidth, subHeight);
                if (data == nullptr)
                {
                    for (auto *buffer : dataBuffers)
                        delete[] buffer;
                    dataBuffers.clear();
                    std::cerr << "Failed to allocate memory for image buffer." << std::endl;
                    return -1;
                }
                else
                    dataBuffers.push_back(data);
                // Background tempBackground;
                // backgrounds.push_back(tempBackground);

                ImageParams parameters = {data,
                                          subWidth,
                                          subHeight,
                                          0,
                                          0,
                                          subWidth,
                                          subHeight,
                                          INIT_KEEP / PARTITION_THREADS,
                                          &backgrounds[partitionIndex]};

                // std::cout << "创建分区 [" << i * horizontalPartitions + j + 1 << "/"
                // << horizontalPartitions * verticalPartitions << "]" << std::endl;

                futures.push_back(std::async(std::launch::async, &AstroExtractorSolver::extractPartition, this, parameters));
                partitionIndex++;
            }
        }

        // std::cout << "已启动所有分区处理任务，等待完成..." << std::endl;
    }
    else
    {
        std::cout << "图像较小，使用单分区处理模式" << std::endl;

        uint32_t startX, startY, subWidth, subHeight;
        computeMargin(x, y, x + w - 1, y + h - 1, imagestats.width, imagestats.height, default_margin,
                      &startX, &startY, &subWidth, &subHeight);

        float *data = allocateFloatBuffer(startX, startY, subWidth, subHeight);
        if (data == nullptr)
        {
            for (auto *buffer : dataBuffers)
            {
                if (buffer)
                    delete[] buffer;
                buffer = nullptr;
            }
            dataBuffers.clear();
            std::cerr << "Failed to allocate memory for image buffer." << std::endl;
            return -1;
        }
        if (data)
            dataBuffers.push_back(data);
        startupOffsets.push_back(StartupOffset(startX, startY, subWidth, subHeight, x, y, x + w - 1, y + h - 1));
        // Background tempBackground;
        // backgrounds.push_back(tempBackground);
        backgrounds.resize(1); // 为单分区模式分配一个元素

        ImageParams parameters = {data, subWidth, subHeight, 0, 0, subWidth, subHeight, static_cast<uint32_t>(INIT_KEEP), &backgrounds[backgrounds.size() - 1]};
        futures.push_back(std::async(std::launch::async, &AstroExtractorSolver::extractPartition, this, parameters));
    }

    std::cout << "正在收集各分区的提取结果..." << std::endl;

    for (size_t i = 0; i < futures.size(); ++i)
    {
        // std::cout << "处理中分区 [" << i + 1 << "/" << futures.size() << "]..." << std::endl;
        // 1. 获取 future 的结果
        std::vector<Star> partitionStars = futures[i].get();
        std::vector<Star> acceptedStars;

        // 2. 检查索引是否有效，并获取对应的 offset
        if (i < startupOffsets.size())
        {
            // 3. 使用索引直接访问，而不是 takeFirst() 或 front()
            const StartupOffset &oneOffset = startupOffsets[i];
            const int startX = oneOffset.startX;
            const int startY = oneOffset.startY;
            for (auto &oneStar : partitionStars)
            {
                // Don't use stars from the margins (they're detected in other partitions).
                if (oneStar.x < (oneOffset.innerStartX - startX) ||
                    oneStar.y < (oneOffset.innerStartY - startY) ||
                    oneStar.x > (oneOffset.innerEndX - startX) ||
                    oneStar.y > (oneOffset.innerEndY - startY))
                    continue;
                oneStar.x += startX;
                oneStar.y += startY;
                acceptedStars.push_back(oneStar);
            }
        }
        extractedStars.insert(extractedStars.end(), acceptedStars.begin(), acceptedStars.end());

        // std::cout << "分区 [" << i + 1 << "/" << futures.size() << "] 完成，接受 "
        //<< acceptedStars.size() << " 颗恒星" << std::endl;
    }

    std::cout << "计算背景统计信息..." << std::endl;

    double sumGlobal = 0, sumRmsSq = 0;
    for (const auto &bg : std::as_const(backgrounds))
    {
        sumGlobal += bg.global;
        sumRmsSq += bg.globalrms * bg.globalrms;
    }
    if (!backgrounds.empty())
    {
        background.bw = backgrounds[0].bw;
        background.bh = backgrounds[0].bh;
    }
    background.num_stars_detected = extractedStars.size();
    background.global = sumGlobal / backgrounds.size();
    background.globalrms = sqrt(sumRmsSq / backgrounds.size());

    std::cout << "对提取的恒星按亮度排序..." << std::endl;

    std::sort(extractedStars.begin(), extractedStars.end(), [](const Star &s1, const Star &s2)
              { return s1.mag < s2.mag; });

    for (auto *buffer : dataBuffers)
    {
        if (buffer)
            delete[] buffer;
    }
    dataBuffers.clear();
    futures.clear();

    hasExtracted = true;

    std::cout << "恒星提取完成！共提取到 " << extractedStars.size() << " 颗恒星" << std::endl;

    return 0;
}

int AstroExtractorSolver::solve(const std::string indexPath, const double focalLength)
{
    engine_t *engine = engine_new();

    // This sets some basic engine settings
    engine->inparallel = TRUE;
    engine->minwidth = 0.1;
    engine->maxwidth = 180.0;
    log_init((log_level)LOG_NONE);

    engine_add_search_path(engine, indexPath.c_str());
    engine_autoindex_search_paths(engine);

    if (!pl_size(engine->indexes))
    {
        std::cerr << "No indexes found in path: " << indexPath << std::endl;
        engine_free(engine);
        engine = nullptr;
        return -1;
    }

    prepareJob(focalLength);

    blind_t *bp = &(job->bp);

    // This will set up the field file to solve as an xylist
    double *xArray = nullptr;
    double *yArray = nullptr;
    try
    {
        xArray = new double[extractedStars.size()];
        yArray = new double[extractedStars.size()];
    }
    catch (std::bad_alloc &)
    {
        if (xArray)
            delete[] xArray;
        if (yArray)
            delete[] yArray;
        std::cerr << "Failed to allocate memory for star position arrays." << std::endl;
        return -1;
    }

    int i = 0;
    for (const auto &oneStar : extractedStars)
    {
        xArray[i] = oneStar.x;
        yArray[i] = oneStar.y;
        i++;
    }

    starxy_t *fieldToSolve = (starxy_t *)calloc(1, sizeof(starxy_t));
    fieldToSolve->x = xArray;
    fieldToSolve->y = yArray;
    fieldToSolve->N = extractedStars.size();
    fieldToSolve->flux = nullptr;
    fieldToSolve->background = nullptr;
    bp->solver.fieldxy = fieldToSolve;

    // This sets the depths for the job.
    if (!il_size(engine->default_depths))
    {
        for (int i = 10; i < 210; i += 10)
            il_append(engine->default_depths, i);
    }
    if (il_size(job->depths) == 0)
    {
        if (engine->inparallel)
        {
            // no limit.
            il_append(job->depths, 0);
            il_append(job->depths, 0);
        }
        else
            il_append_list(job->depths, engine->default_depths);
    }

    if (engine->minwidth <= 0.0 || engine->maxwidth <= 0.0 || engine->minwidth > engine->maxwidth)
    {
        std::cerr << "Invalid min/max width settings." << std::endl;
        return -1;
    }

    /// This sets the scales based on the minwidth and maxwidth if the image scale isn't known
    if (!dl_size(job->scales))
    {
        double arcsecperpix;
        arcsecperpix = deg2arcsec(engine->minwidth) / imagestats.width;
        dl_append(job->scales, arcsecperpix);
        arcsecperpix = deg2arcsec(engine->maxwidth) / imagestats.height;
        dl_append(job->scales, arcsecperpix);
    }

    bp->timelimit = 30;
    bp->cpulimit = 30;

    std::cout << "开始进行天体定位求解..." << std::endl;
    // This runs the job in the engine in the file engine.c
    if (engine_run_job(engine, job))
        std::cerr << "求解失败！" << std::endl;

    // This deletes or frees the items that are no longer needed.
    engine_free(engine);
    engine = nullptr;
    bl_free(job->scales);
    job->scales = nullptr;
    dl_free(job->depths);
    job->depths = nullptr;
    free(fieldToSolve);
    fieldToSolve = nullptr;
    if (xArray)
        delete[] xArray;
    xArray = nullptr;
    if (yArray)
        delete[] yArray;
    yArray = nullptr;

    // Note: I can only get these items after the solve because I made a couple of small changes to the Astrometry.net Code.
    // I made it return in solve_fields in blind.c before it ran "cleanup".  I also had it wait to clean up solutions, blind and solver in engine.c.  We will do that after we get the solution information.

    match = bp->solver.best_match;
    int returnCode = 0;

    if (match.sip)
    {
        wcs = *match.sip;
        hasWCS = true;
        double ra, dec, fieldw, fieldh, pixscale;
        char rastr[32], decstr[32];
        Parity parity;
        char *fieldunits;

        double orient;
        sip_get_radec_center(&wcs, &ra, &dec);
        sip_get_radec_center_hms_string(&wcs, rastr, decstr);
        sip_get_field_size(&wcs, &fieldw, &fieldh, &fieldunits);
        orient = sip_get_orientation(&wcs);

        // Note, negative determinant = positive parity.
        double det = sip_det_cd(&wcs);
        parity = (det < 0 ? POSITIVE : NEGATIVE);
        pixscale = sip_pixel_scale(&wcs);

        double raErr = 0;
        double decErr = 0;

        if (strcmp(fieldunits, "degrees") == 0)
        {
            fieldw *= 60;
            fieldh *= 60;
        }
        if (strcmp(fieldunits, "arcseconds") == 0)
        {
            fieldw /= 60;
            fieldh /= 60;
        }

        solution = {fieldw, fieldh, ra, dec, orient, pixscale, parity, raErr, decErr};
        solutionIndexNumber = match.indexid;
        solutionHealpix = match.healpix;
        hasSolved = true;
        returnCode = 0;

        std::cout << "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
        std::cout << "解算对数优势比 (Log Odds):  " << bp->solver.best_logodds << std::endl;
        std::cout << "匹配数量:  " << match.nmatch << std::endl;
        std::cout << "使用的索引文件ID:  " << match.indexid << std::endl;
        std::cout << "视场中心: (赤经,赤纬) = (" << ra << ", " << dec << ") 度." << std::endl;
        std::cout << "视场中心: (赤经 时:分:秒, 赤纬 度:分:秒) = (" << rastr << ", " << decstr << ")." << std::endl;
        std::cout << "视场大小: " << fieldw << " x " << fieldh << " 角分" << std::endl;
        std::cout << "像素尺度: " << pixscale << "\"" << std::endl;
        std::cout << "视场旋转角度: 天顶向东偏 " << orient << " 度" << std::endl;
    }
    else
    {
        std::cerr << "求解失败，未找到匹配！" << std::endl;
        returnCode = -1;
    }

    // This code was taken from engine.c and blind.c so that we can clean up all of the allocated memory after we get the solution information out of it so that we can prevent memory leaks.

    for (size_t i = 0; i < bl_size(bp->solutions); i++)
    {
        MatchObj *mo = (MatchObj *)bl_access(bp->solutions, i);
        // verify_free_matchobj(mo); // redundent with below
        blind_free_matchobj(mo);
    }
    // bl_remove_all(bp->solutions); // redundent with below
    // blind_clear_solutions(bp); // calls bl_remove_all(bp->solutions), redundent
    solver_cleanup(&bp->solver);
    blind_cleanup(bp); // calls bl_free(bp->solutions) which calls bl_remove_all(solutions)

    return returnCode;
}

void AstroExtractorSolver::print_stars()
{
    if (!hasExtracted)
    {
        std::cout << "尚未提取恒星，请先调用 extract_stars()" << std::endl;
        return;
    }

    if (extractedStars.empty())
    {
        std::cout << "未检测到任何恒星" << std::endl;
        return;
    }

    std::cout << "\n========== 提取的恒星列表 ==========" << std::endl;
    std::cout << "总数: " << extractedStars.size() << " 颗恒星" << std::endl;
    std::cout << "背景: global=" << background.global
              << ", globalrms=" << background.globalrms << std::endl;
    std::cout << "\n"
              << std::setw(5) << "序号"
              << std::setw(10) << "X坐标"
              << std::setw(10) << "Y坐标"
              << std::setw(10) << "星等"
              << std::setw(12) << "通量"
              << std::setw(10) << "峰值"
              << std::setw(10) << "HFR"
              << std::setw(8) << "A轴"
              << std::setw(8) << "B轴"
              << std::setw(10) << "角度(°)"
              << std::setw(8) << "像素数" << std::endl;
    std::cout << std::string(110, '-') << std::endl;

    int displayCount = std::min(50, static_cast<int>(extractedStars.size()));
    for (int i = 0; i < displayCount; i++)
    {
        const Star &star = extractedStars[i];
        std::cout << std::setw(5) << (i + 1)
                  << std::fixed << std::setprecision(2)
                  << std::setw(10) << star.x
                  << std::setw(10) << star.y
                  << std::setw(10) << star.mag
                  << std::setw(12) << star.flux
                  << std::setw(10) << star.peak
                  << std::setw(10) << star.HFR
                  << std::setw(8) << star.a
                  << std::setw(8) << star.b
                  << std::setw(10) << star.theta
                  << std::setw(8) << star.numPixels << std::endl;
    }

    if (extractedStars.size() > displayCount)
    {
        std::cout << "... 省略了 " << (extractedStars.size() - displayCount)
                  << " 颗恒星" << std::endl;
    }
    std::cout << "====================================\n"
              << std::endl;
}

Solution AstroExtractorSolver::getSolution()
{
    return solution;
}

// This function adds a margin around the rectangle with corners x1,y1 x2,y2 for the image
// with the given width and height, making sure the margin doesn't go outside the image.
// It returns the expanded rectangle defined by corner startX,startY and width, height.
void AstroExtractorSolver::computeMargin(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t imageWidth, uint32_t imageHeight, uint32_t margin, uint32_t *startX, uint32_t *startY, uint32_t *width, uint32_t *height)
{
    // Figure out the start and start margins.
    // Make sure these are signed operations.
    int tempStartX = ((int)x1) - ((int)margin);
    if (tempStartX < 0)
    {
        tempStartX = 0;
    }
    *startX = tempStartX;

    int tempStartY = ((int)y1) - ((int)margin);
    if (tempStartY < 0)
    {
        tempStartY = 0;
    }
    *startY = tempStartY;

    // Figure out the end and end margins.
    uint32_t endX = x2 + margin;
    if (endX >= imageWidth)
        endX = imageWidth - 1;
    uint32_t endY = y2 + margin;
    if (endY >= imageHeight)
        endY = imageHeight - 1;

    *width = endX - *startX + 1;
    *height = endY - *startY + 1;
}

float *AstroExtractorSolver::allocateFloatBuffer(int x, int y, int w, int h)
{
    float *buffer = nullptr;
    try
    {
        buffer = new float[w * h];
    }
    catch (std::bad_alloc &)
    {
        std::cerr << "Failed to allocate memory for image buffer." << std::endl;
        return nullptr;
    }

    int channelShift = imagestats.samples_per_channel * imagestats.bytes_per_pixel;
    auto *rawBuffer = reinterpret_cast<uint8_t const *>(imageBuffer + channelShift);
    float *floatPtr = buffer;

    int x2 = x + w;
    int y2 = y + h;

    for (int y1 = y; y1 < y2; y1++)
    {
        int offset = y1 * imagestats.width;
        for (int x1 = x; x1 < x2; x1++)
        {
            *floatPtr++ = rawBuffer[offset + x1];
        }
    }

    return buffer;
}

std::vector<Star> AstroExtractorSolver::extractPartition(const ImageParams &parameters)
{
    std::vector<float> convFilter = {1, 2, 1,
                                     2, 4, 2,
                                     1, 2, 1};
    float *imback = nullptr;
    double *fluxerr = nullptr, *area = nullptr;
    short *flag = nullptr;
    int status = 0;
    sep_bkg *bkg = nullptr;
    sep_catalog *catalog = nullptr;
    std::vector<Star> partitionStars;
    const uint32_t maxRadius = 50;

    auto cleanup = [&]()
    {
        sep_bkg_free(bkg);
        bkg = nullptr;
        Extract::sep_catalog_free(catalog);
        catalog = nullptr;
        free(imback);
        imback = nullptr;
        free(fluxerr);
        fluxerr = nullptr;
        free(area);
        area = nullptr;
        free(flag);
        flag = nullptr;

        if (status != 0)
        {
            char errorMessage[512];
            sep_get_errmsg(status, errorMessage);
            std::cerr << "SEP Error: " << errorMessage << std::endl;
        }
    };

    // These are for the HFR
    double requested_frac[2] = {0.5, 0.99};
    double flux_fractions[2] = {0};
    short flux_flag = 0;
    std::vector<std::pair<int, double>> ovals;
    int numToProcess = 0;

    // #0 Create SEP Image structure
    sep_image im = {parameters.data,
                    nullptr,
                    nullptr,
                    nullptr,
                    SEP_TFLOAT,
                    0,
                    0,
                    0,
                    static_cast<int>(parameters.width),
                    static_cast<int>(parameters.height),
                    static_cast<int>(parameters.subW),
                    static_cast<int>(parameters.subH),
                    0,
                    SEP_NOISE_NONE,
                    1.0,
                    0};

    // #1 Background estimate
    status = sep_background(&im, 64, 64, 3, 3, 0.0, &bkg);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    // #2 Background evaluation
    imback = (float *)malloc((parameters.subW * parameters.subH) * sizeof(float));
    status = sep_bkg_array(bkg, imback, SEP_TFLOAT);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    // Saving some background information
    parameters.background->bh = bkg->bh;
    parameters.background->bw = bkg->bw;
    parameters.background->global = bkg->global;
    parameters.background->globalrms = bkg->globalrms;

    // #3 Background subtraction
    status = sep_bkg_subarray(bkg, im.data, im.dtype);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    std::unique_ptr<Extract> extractor;
    extractor.reset(new Extract());
    // #4 Source Extraction
    // Note that we set deblend_cont = 1.0 to turn off deblending.
    const double extractionThreshold = THRESHOLD_BG_MULTIPLE * bkg->globalrms;
    // fprintf(stderr, "Using %.1f =  %.1f * %.1f + %.1f\n", extractionThreshold, m_ActiveParameters.threshold_bg_multiple, bkg->globalrms,  m_ActiveParameters.threshold_offset);
    status = extractor->sep_extract(&im, extractionThreshold, SEP_THRESH_ABS, MIN_AREA,
                                    convFilter.data(),
                                    sqrt(convFilter.size()), sqrt(convFilter.size()), SEP_FILTER_CONV,
                                    32,
                                    0.005, 1, 1, &catalog);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    // Record the number of stars detected.
    parameters.background->num_stars_detected = catalog->nobj;

    // Find the oval sizes for each detection in the detected star catalog, and sort by that. Oval size
    // correlates very well with HFR and likely magnitude.
    for (int i = 0; i < catalog->nobj; i++)
    {
        const double ovalSizeSq = catalog->a[i] * catalog->a[i] + catalog->b[i] * catalog->b[i];
        ovals.push_back(std::pair<int, double>(i, ovalSizeSq));
    }
    if (catalog->nobj > 0)
        std::sort(ovals.begin(), ovals.end(), [](const std::pair<int, double> &o1, const std::pair<int, double> &o2) -> bool
                  { return o1.second > o2.second; });

    numToProcess = std::min(static_cast<uint32_t>(catalog->nobj), parameters.keep);
    for (int index = 0; index < numToProcess; index++)
    {
        // Processing detections in the order of the sort above.
        int i = ovals[index].first;

        if (catalog->flag[i] & SEP_OBJ_TRUNC)
        {
            // Don't accept detections that go over the boundary.
            continue;
        }

        // Variables that are obtained from the catalog
        // FOR SOME REASON, I FOUND THAT THE POSITIONS WERE OFF BY 1 PIXEL??
        // This might be because of this: https://sextractor.readthedocs.io/en/latest/Param.html
        //" Following the FITS convention, in SExtractor the center of the first image pixel has coordinates (1.0,1.0). "
        float xPos = catalog->x[i] + 1;
        float yPos = catalog->y[i] + 1;
        float a = catalog->a[i];
        float b = catalog->b[i];
        float theta = catalog->theta[i];
        float cxx = catalog->cxx[i];
        float cyy = catalog->cxx[i];
        float cxy = catalog->cxy[i];
        double flux = catalog->flux[i];
        double peak = catalog->peak[i];
        int numPixels = catalog->npix[i];

        // Variables that will be obtained through methods
        double kronrad;
        short kron_flag;
        double sum = 0;
        double sumerr;
        double kron_area;

        bool use_circle;

        sep_sum_circle(&im, xPos, yPos, 3.5, 0, 5, 0, &sum,
                       &sumerr, &kron_area, &kron_flag);

        float mag = 20 - 2.5 * log10(sum);
        float HFR = 0;

        // Get HFR
        sep_flux_radius(&im, catalog->x[i], catalog->y[i], maxRadius, 0, 5, 0, &flux, requested_frac, 2,
                        flux_fractions,
                        &flux_flag);
        HFR = flux_fractions[0];

        Star oneStar = {xPos,
                        yPos,
                        mag,
                        static_cast<float>(sum),
                        static_cast<float>(peak),
                        HFR,
                        a,
                        b,
                        theta * (180.0 / M_PI),
                        0,
                        0,
                        numPixels};
        // Make a copy and add it to vector
        partitionStars.push_back(oneStar);
    }

    cleanup();

    return partitionStars;
}

void AstroExtractorSolver::prepareJob(double focalLength)
{
    blind_t *bp = &(job->bp);
    solver_t *sp = &(bp->solver);

    job->scales = dl_new(8);
    job->depths = il_new(8);

    job->use_radec_center = FALSE;

    // These initialize the blind and solver objects, and they MUST be in this order according to astrometry.net
    blind_init(bp);
    solver_set_default_values(sp);

    // These set the width and the height of the image in the solver
    sp->field_maxx = imagestats.width;
    sp->field_maxy = imagestats.height;

    // We would like the Coordinates found to be the center of the image
    sp->set_crpix = TRUE;
    sp->set_crpix_center = TRUE;

    // Logratios for Solving
    bp->logratio_tosolve = log(1e9);
    sp->logratio_tokeep = log(1e9);
    sp->logratio_totune = log(1e9);
    sp->logratio_bail_threshold = log(DEFAULT_BAIL_THRESHOLD);

    bp->best_hit_only = TRUE;

    // gotta keep it to solve it!
    sp->logratio_tokeep = MIN(sp->logratio_tokeep, bp->logratio_tosolve);

    job->include_default_scales = 0;
    sp->parity = 2;

    // These set the default tweak settings
    sp->do_tweak = TRUE;
    sp->tweak_aborder = 2;
    sp->tweak_abporder = 2;

    if (focalLength > 0)
    {
        double appu, appl;
        appu = rad2arcsec(atan(36. / (2. * (focalLength - 10)))) / (double)imagestats.width * 2.;
        appl = rad2arcsec(atan(36. / (2. * (focalLength + 10)))) / (double)imagestats.width * 2.;
        // appu = 5.99;
        // appl = 5.13;
        std::cout << "Using approximate image scale range of " << appl << " to " << appu << " arcsec/pixel based on focal length of " << focalLength << " mm." << std::endl;

        dl_append(job->scales, appl);
        dl_append(job->scales, appu);
        blind_add_field_range(bp, appl, appu);
    }

    blind_add_field(bp, 1);
}
