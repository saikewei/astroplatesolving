#ifndef EXTRACTOR_SOLVER_H
#define EXTRACTOR_SOLVER_H

#include <stdint.h>
#include <stddef.h>
#include <image_loader.h>
#include <vector>
#include <string>
#include <future>

extern "C"
{
#include "astrometry/engine.h"
}

const double THRESHOLD_BG_MULTIPLE = 2.0;
const double MIN_AREA = 10.0;
const double DEBLEND_CONTRAST = 0.005;
const double MAX_SIZE = 0, MIN_SIZE = 0, MAX_ELLIPSE = 0;
const double INIT_KEEP = 1000000;
const int PARTITION_THREADS = 4;
const int PARTITION_SIZE = 200;

class StartupOffset
{
public:
    int startX = 0, startY = 0, width = 0, height = 0;
    int innerStartX = 0, innerStartY = 0, innerEndX = 0, innerEndY = 0;
    StartupOffset(int x, int y, int w, int h, int inX1, int inY1, int inX2, int inY2)
        : startX(x), startY(y), width(w), height(h),
          innerStartX(inX1), innerStartY(inY1), innerEndX(inX2), innerEndY(inY2) {}
};

typedef struct Background
{
    int bw, bh;             // single tile width, height
    float global;           // global mean
    float globalrms;        // global sigma
    int num_stars_detected; // Number of stars detected before any reduction.
} Background;

typedef struct
{
    float *data;
    uint32_t width;
    uint32_t height;
    uint32_t subX;
    uint32_t subY;
    uint32_t subW;
    uint32_t subH;
    uint32_t keep;
    Background *background;
} ImageParams;

typedef struct Star
{
    float x;       // The x position of the star in Pixels
    float y;       // The y position of the star in Pixels
    float mag;     // The magnitude of the star, note that this is a relative magnitude based on the star extraction options.
    float flux;    // The calculated total flux
    float peak;    // The peak value of the star
    float HFR;     // The half flux radius of the star
    float a;       // The semi-major axis of the star
    float b;       // The semi-minor axis of the star
    float theta;   // The angle of orientation of the star
    float ra;      // The right ascension of the star
    float dec;     // The declination of the star
    int numPixels; // The number of pixels occupied by the star in the image.
} Star;

typedef enum
{
    POSITIVE,
    NEGATIVE,
    BOTH
} Parity;

// This struct contains information about the astrometric solution
// for an image.
typedef struct Solution
{
    double fieldWidth;  // The calculated width of the field in arcminutes
    double fieldHeight; // The calculated height of the field in arcminutes
    double ra;          // The Right Ascension of the center of the field in degrees
    double dec;         // The Declination of the center of the field in degrees
    double orientation; // The orientation angle of the image from North in degrees
    double pixscale;    // The pixel scale of the image in arcseconds per pixel
    Parity parity;      // The parity of the solved image. (Whether it has been flipped)  JPEG images tend to have negative parity while FITS files tend to have positive parity.
    double raError;     // The error between the search_ra position and the solution ra position in arcseconds
    double decError;    // The error between the search_dec position and the solution dec position in arcseconds
} Solution;

class AstroExtractorSolver
{
public:
    AstroExtractorSolver(const ImageStats &_imagestats, uint8_t const *_imageBuffer);
    int extract_stars();
    int solve(const std::string indexPath, const double focalLength);
    void print_stars();
    Solution getSolution();

private:
    ImageStats imagestats;
    uint8_t const *imageBuffer;
    std::vector<std::future<std::vector<Star>>> futures;
    std::vector<Star> extractedStars;
    Background background;
    bool hasExtracted;
    bool hasWCS;
    bool hasSolved;
    job_t thejob; // This is the job file that will be created for astrometry.net to solve
    job_t *job;   // This is a pointer to that job file
    // Solution related
    MatchObj match; // This is where the match object gets stored once the solving is done.
    sip_t wcs;      // This is where the WCS data gets saved once the solving is done
    Solution solution;
    short solutionIndexNumber; // This is the index number of the index used to solve the image.
    short solutionHealpix;     // This is the healpix of the index used to solve the image.

    void computeMargin(uint32_t x1,
                       uint32_t y1,
                       uint32_t x2,
                       uint32_t y2,
                       uint32_t imageWidth,
                       uint32_t imageHeight,
                       uint32_t margin,
                       uint32_t *startX,
                       uint32_t *startY,
                       uint32_t *width,
                       uint32_t *height);

    float *allocateFloatBuffer(int x, int y, int w, int h);
    std::vector<Star> extractPartition(const ImageParams &parameters);
    void prepareJob(double focalLength = 0);
};

#endif // EXTRACTOR_SOLVER_H