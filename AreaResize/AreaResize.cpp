/*
    AreaResize.dll

    Copyright (C) 2012 Oka Motofumi(chikuzen.mo at gmail dot com)

    author : Oka Motofumi
    VapourSynth port : Kiyamou

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <string>
#include <memory>

#if defined(_MSC_VER)
#include <ppl.h>
#endif

#include "VapourSynth.h"
#include "VSHelper.h"

#define RGB_PIXEL_RANGE_EXTENDED 25501 // for 8bit RGB, 25501 = (256 - 1) * 100 + 1
//#define DOUBLE_ROUND_MAGIC_NUMBER 6755399441055744.0

struct AreaData {
    VSNodeRef* node;
    const VSVideoInfo* vi;
    int target_width, target_height;
    double* linear_LUT;
    double* gamma_LUT;
};

static int gcd(int x, int y) {
    int m = x % y;
    return m == 0 ? y : gcd(y, m);
}

template <typename T>
static bool ResizeHorizontalPlanar(const VSFrameRef* src, VSFrameRef* dst, const T* srcp, T* VS_RESTRICT dstp,
    int src_stride, int dst_stride, int plane, const AreaData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    int gcd_h = gcd(vsapi->getFrameWidth(src, plane), vsapi->getFrameWidth(dst, plane));
    int num = vsapi->getFrameWidth(dst, plane) / gcd_h;
    int den = vsapi->getFrameWidth(src, plane) / gcd_h;
    double invert_den = 1.0 / (double)den;

    int src_height = vsapi->getFrameHeight(src, plane);
    int dst_width = vsapi->getFrameWidth(dst, plane);

#if defined(_MSC_VER)
    Concurrency::parallel_for(0, (int)src_height, [&](int curPixel)
#else
    for (int curPixel = 0; curPixel < src_height; curPixel++)
#endif
    {
        const T* curSrcp = srcp + (src_stride * curPixel);  // same as "srcp += src_stride"
        T* curBuff = dstp + (dst_width * curPixel);

        unsigned short count_num = num;
        int index_src = 0;
        for (unsigned int index_value = dst_width; index_value--; )
        {
            double pixel = 0.0;
            unsigned short count_den = den;
            while (count_den > 0)
            {
                short left = count_num - count_den;
                unsigned short partial;
                if (left <= 0)
                {
                    partial = count_num;
                    count_den = (int)abs(left);
                    count_num = num;
                }
                else
                {
                    count_num = left;
                    partial = count_den;
                    count_den = 0;
                }

                pixel += (double)curSrcp[index_src] * partial;

                if (left <= 0)
                    index_src++;
            }

            const int index = (dst_width - index_value - 1);
            //double pixelConvert = (pixel * invert_den) + DOUBLE_ROUND_MAGIC_NUMBER;
            //const T pixelValue = (T)reinterpret_cast<int&>(pixelConvert);
            T pixelValue = (T)(pixel * invert_den);
            curBuff[index] = pixelValue;
        }
#if defined(_MSC_VER)
    });
#else
    }
#endif

    return true;
}

template <typename T>
static bool ResizeVerticalPlanar(const VSFrameRef* src, VSFrameRef* dst, const T* srcp, T* VS_RESTRICT dstp,
    int src_stride, int dst_stride, int plane, const AreaData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    int gcd_v = gcd(vsapi->getFrameHeight(src, plane), vsapi->getFrameHeight(dst, plane));
    int num = vsapi->getFrameHeight(dst, plane) / gcd_v;
    int den = vsapi->getFrameHeight(src, plane) / gcd_v;
    double invert_den = 1.0 / (double)den;

    int dst_width = vsapi->getFrameWidth(dst, plane);
    int dst_height = vsapi->getFrameHeight(dst, plane);

#if defined(_MSC_VER)
    Concurrency::parallel_for(0, (int)dst_width, [&](int curPixel)
#else
    for (int curPixel = 0; curPixel < dst_width; curPixel++)
#endif
    {
        const T* curSrcp = srcp + curPixel;
        T* curDstp = dstp + curPixel;

        unsigned short count_num = num;
        int index_src = 0;
        for (unsigned int index_value = dst_height; index_value--; )
        {
            double pixel = 0.0;
            unsigned short count_den = den;
            while (count_den > 0)
            {
                short left = count_num - count_den;
                unsigned short partial;
                if (left <= 0)
                {
                    partial = count_num;
                    count_den = (int)abs(left);
                    count_num = num;
                }
                else
                {
                    count_num = left;
                    partial = count_den;
                    count_den = 0;
                }

                pixel += (double)curSrcp[index_src] * partial;

                if (left <= 0)
                    index_src += src_stride;
            }

            const int index = (dst_height - index_value - 1) * dst_stride;
            //double pixelConvert = (pixel * invert_den) + DOUBLE_ROUND_MAGIC_NUMBER;
            //const T pixelValue = (T)reinterpret_cast<int&>(pixelConvert);
            T pixelValue = (T)(pixel * invert_den);
            curDstp[index] = pixelValue;
        }
#if defined(_MSC_VER)
    });
#else
    }
#endif

    return true;
}

template <typename T>
static bool ResizeHorizontalRGB(const VSFrameRef* src, VSFrameRef* dst, const T* srcp, T* VS_RESTRICT dstp,
    int src_stride, int dst_stride, const AreaData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    int gcd_h = gcd(vsapi->getFrameWidth(src, 0), vsapi->getFrameWidth(dst, 0));
    int num = vsapi->getFrameWidth(dst, 0) / gcd_h;
    int den = vsapi->getFrameWidth(src, 0) / gcd_h;

    int src_height = vsapi->getFrameHeight(src, 0);
    int dst_width = vsapi->getFrameWidth(dst, 0);

    double scale;
    if (d->vi->format->bytesPerSample == 1)
        scale = 100.0;
    else if (d->vi->format->bytesPerSample == 2)
        scale = 1.0;

    double invert_den_hun = scale / (double)den;

    const int ps = 3;
#if defined(_MSC_VER)
    Concurrency::parallel_for(0, (int)src_height, [&](int curPixel)
#else
    for (int curPixel = 0; curPixel < src_height; curPixel++)
#endif
    {
        const T* curSrcp = srcp + (src_stride * curPixel * ps);
        T* curBuff = dstp + (dst_width * curPixel * ps);

        unsigned short count_num = num;
        int index_src = 0;
        for (unsigned int index_value = dst_width; index_value--; )
        {
            double blue = 0.0;
            double green = 0.0;
            double red = 0.0;
            unsigned short count_den = den;
            while (count_den > 0)
            {
                short left = count_num - count_den;
                unsigned short partial;
                if (left <= 0)
                {
                    partial = count_num;
                    count_den = (int)abs(left);
                    count_num = num;
                }
                else
                {
                    count_num = left;
                    partial = count_den;
                    count_den = 0;
                }

                blue += (d->linear_LUT[int(curSrcp[index_src])]) * partial;
                green += (d->linear_LUT[int(curSrcp[index_src + 1])]) * partial;
                red += (d->linear_LUT[int(curSrcp[index_src + 2])]) * partial;

                if (left <= 0)
                    index_src += ps;
            }

            const int index = (dst_width - index_value - 1) * ps;

            T blueValue, greenValue, redValue;
            blueValue = (T)(d->gamma_LUT[int(blue * invert_den_hun)]);
            greenValue = (T)(d->gamma_LUT[int(green * invert_den_hun)]);
            redValue = (T)(d->gamma_LUT[int(red * invert_den_hun)]);

            curBuff[index] = blueValue;
            curBuff[index + 1] = greenValue;
            curBuff[index + 2] = redValue;
        }
#if defined(_MSC_VER)
    });
#else
    }
#endif

    return true;
}

template <typename T>
static bool ResizeVerticalRGB(const VSFrameRef* src, VSFrameRef* dst, const T* srcp, T* VS_RESTRICT dstp,
    int src_stride, int dst_stride, const AreaData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    int gcd_v = gcd(vsapi->getFrameHeight(src, 0), vsapi->getFrameHeight(dst, 0));
    int num = vsapi->getFrameHeight(dst, 0) / gcd_v;
    int den = vsapi->getFrameHeight(src, 0) / gcd_v;

    int dst_height = vsapi->getFrameHeight(dst, 0);
    int dst_width = vsapi->getFrameWidth(dst, 0);

    double scale;
    if (d->vi->format->bytesPerSample == 1)
        scale = 100.0;
    else if (d->vi->format->bytesPerSample == 2)
        scale = 1.0;

    double invert_den_hun = scale / (double)den;

    const int ps = 3;
#if defined(_MSC_VER)
    Concurrency::parallel_for(0, (int)dst_width, [&](int curPixel)
#else
    for (int curPixel = 0; curPixel < dst_width; curPixel++)
#endif
    {
        const T* curSrcp = srcp + curPixel * ps;
        T* curDstp = dstp + curPixel * ps;

        unsigned short count_num = num;
        int index_src = 0;
        for (int index_value = dst_height; index_value--; )
        {
            double blue = 0.0;
            double green = 0.0;
            double red = 0.0;
            unsigned short count_den = den;
            while (count_den > 0)
            {
                short left = count_num - count_den;
                unsigned short partial;
                if (left <= 0)
                {
                    partial = count_num;
                    count_den = (int)abs(left);
                    count_num = num;
                }
                else
                {
                    count_num = left;
                    partial = count_den;
                    count_den = 0;
                }

                blue += d->linear_LUT[int(curSrcp[index_src])] * partial;
                green += d->linear_LUT[int(curSrcp[index_src + 1])] * partial;
                red += d->linear_LUT[int(curSrcp[index_src + 2])] * partial;

                if (left <= 0)
                    index_src += src_stride * ps;
            }

            const int index = (dst_height - index_value - 1) * dst_stride * ps;

            T blueValue, greenValue, redValue;
            blueValue = (T)(d->gamma_LUT[int(blue * invert_den_hun)]);
            greenValue = (T)(d->gamma_LUT[int(green * invert_den_hun)]);
            redValue = (T)(d->gamma_LUT[int(red * invert_den_hun)]);

            curDstp[index] = blueValue;
            curDstp[index + 1] = greenValue;
            curDstp[index + 2] = redValue;
        }
#if defined(_MSC_VER)
    });
#else
    }
#endif

    return true;
}

template <>
bool ResizeHorizontalRGB(const VSFrameRef* src, VSFrameRef* dst, const float* srcp, float* VS_RESTRICT dstp,
    int src_stride, int dst_stride, const AreaData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    int gcd_h = gcd(vsapi->getFrameWidth(src, 0), vsapi->getFrameWidth(dst, 0));
    int num = vsapi->getFrameWidth(dst, 0) / gcd_h;
    int den = vsapi->getFrameWidth(src, 0) / gcd_h;
    double invert_den_hun = 1.0 / (double)den;

    int src_height = vsapi->getFrameHeight(src, 0);
    int dst_width = vsapi->getFrameWidth(dst, 0);

    const int ps = 3;
#if defined(_MSC_VER)
    Concurrency::parallel_for(0, (int)src_height, [&](int curPixel)
#else
    for (int curPixel = 0; curPixel < src_height; curPixel++)
#endif
    {
        const float* curSrcp = srcp + (src_stride * curPixel * ps);
        float* curBuff = dstp + (dst_width * curPixel * ps);

        unsigned short count_num = num;
        int index_src = 0;
        for (unsigned int index_value = dst_width; index_value--; )
        {
            double blue = 0.0;
            double green = 0.0;
            double red = 0.0;
            unsigned short count_den = den;
            while (count_den > 0)
            {
                short left = count_num - count_den;
                unsigned short partial;
                if (left <= 0)
                {
                    partial = count_num;
                    count_den = (int)abs(left);
                    count_num = num;
                }
                else
                {
                    count_num = left;
                    partial = count_den;
                    count_den = 0;
                }

                blue += curSrcp[index_src] * partial;
                green += curSrcp[index_src + 1] * partial;
                red += curSrcp[index_src + 2] * partial;

                if (left <= 0)
                    index_src += ps;
            }

            const int index = (dst_width - index_value - 1) * ps;

            float blueValue, greenValue, redValue;
            blueValue = (float)(blue * invert_den_hun);
            greenValue = (float)(green * invert_den_hun);
            redValue = (float)(red * invert_den_hun);

            curBuff[index] = blueValue;
            curBuff[index + 1] = greenValue;
            curBuff[index + 2] = redValue;
        }
#if defined(_MSC_VER)
    });
#else
    }
#endif

    return true;
}

template <>
bool ResizeVerticalRGB(const VSFrameRef* src, VSFrameRef* dst, const float* srcp, float* VS_RESTRICT dstp,
    int src_stride, int dst_stride, const AreaData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept
{
    int gcd_v = gcd(vsapi->getFrameHeight(src, 0), vsapi->getFrameHeight(dst, 0));
    int num = vsapi->getFrameHeight(dst, 0) / gcd_v;
    int den = vsapi->getFrameHeight(src, 0) / gcd_v;
    double invert_den_hun = 1.0 / (double)den;

    int dst_height = vsapi->getFrameHeight(dst, 0);
    int dst_width = vsapi->getFrameWidth(dst, 0);

    const int ps = 3;
#if defined(_MSC_VER)
    Concurrency::parallel_for(0, (int)dst_width, [&](int curPixel)
#else
    for (int curPixel = 0; curPixel < dst_width; curPixel++)
#endif
    {
        const float* curSrcp = srcp + curPixel * ps;
        float* curDstp = dstp + curPixel * ps;

        unsigned short count_num = num;
        int index_src = 0;
        for (int index_value = dst_height; index_value--; )
        {
            double blue = 0.0;
            double green = 0.0;
            double red = 0.0;
            unsigned short count_den = den;
            while (count_den > 0)
            {
                short left = count_num - count_den;
                unsigned short partial;
                if (left <= 0)
                {
                    partial = count_num;
                    count_den = (int)abs(left);
                    count_num = num;
                }
                else
                {
                    count_num = left;
                    partial = count_den;
                    count_den = 0;
                }

                blue += curSrcp[index_src] * partial;
                green += curSrcp[index_src + 1] * partial;
                red += curSrcp[index_src + 2] * partial;

                if (left <= 0)
                    index_src += src_stride * ps;
            }

            const int index = (dst_height - index_value - 1) * dst_stride * ps;

            float blueValue, greenValue, redValue;
            blueValue = (float)(blue * invert_den_hun);
            greenValue = (float)(green * invert_den_hun);
            redValue = (float)(red * invert_den_hun);

            curDstp[index] = blueValue;
            curDstp[index + 1] = greenValue;
            curDstp[index + 2] = redValue;
        }
#if defined(_MSC_VER)
    });
#else
    }
#endif

    return true;
}

template <typename T>
static void process(const VSFrameRef* src, VSFrameRef* dst, VSFrameRef* buf, const AreaData* const VS_RESTRICT d, const VSAPI* vsapi) noexcept {
    if (d->vi->format->colorFamily == cmYUV)
    {
        for (int plane = 0; plane < d->vi->format->numPlanes; plane++)
        {
            const T* srcp = reinterpret_cast<const T*>(vsapi->getReadPtr(src, plane));
            T* VS_RESTRICT dstp = reinterpret_cast<T*>(vsapi->getWritePtr(dst, plane));
            T* VS_RESTRICT buff = reinterpret_cast<T*>(vsapi->getWritePtr(buf, plane));
            int src_stride = vsapi->getStride(src, plane) / sizeof(T);
            int dst_stride = vsapi->getStride(dst, plane) / sizeof(T);
            int buf_stride = vsapi->getStride(buf, plane) / sizeof(T);

            ResizeHorizontalPlanar<T>(src, dst, srcp, buff, src_stride, buf_stride, plane, d, vsapi);
            ResizeVerticalPlanar<T>(src, dst, (const T*)buff, dstp, buf_stride, dst_stride, plane, d, vsapi);
        }
    }
    else
    {
        const T* srcpR = reinterpret_cast<const T*>(vsapi->getReadPtr(src, 0));
        const T* srcpG = reinterpret_cast<const T*>(vsapi->getReadPtr(src, 1));
        const T* srcpB = reinterpret_cast<const T*>(vsapi->getReadPtr(src, 2));
        int src_stride = vsapi->getStride(src, 0) / sizeof(T);
        int buf_stride = vsapi->getStride(buf, 0) / sizeof(T);
        int dst_stride = vsapi->getStride(dst, 0) / sizeof(T);

        // Interleaved
        T* srcInterleaved = new (std::nothrow) T[d->vi->width * d->vi->height * 3];
        T* dstInterleaved = new (std::nothrow) T[d->target_width * d->target_height * 3];
        T* bufInterleaved = new (std::nothrow) T[d->target_width * d->vi->height * 3];

        // change to Interleaved
        int src_width = vsapi->getFrameWidth(src, 0);
        int src_height = vsapi->getFrameHeight(src, 0);

        for (int y = 0; y < src_height; y++)
        {
            for (int x = 0; x < src_width; x++)
            {
                const unsigned pos = (x + y * src_width) * 3;
                srcInterleaved[pos] = srcpB[x];
                srcInterleaved[pos + 1] = srcpG[x];
                srcInterleaved[pos + 2] = srcpR[x];
            }
            srcpB += src_stride;
            srcpG += src_stride;
            srcpR += src_stride;
        }

        ResizeHorizontalRGB<T>(src, dst, (const T*)srcInterleaved, bufInterleaved, src_stride, buf_stride, d, vsapi);
        ResizeVerticalRGB<T>(src, dst, (const T*)bufInterleaved, dstInterleaved, buf_stride, dst_stride, d, vsapi);

        T* VS_RESTRICT dstpR = reinterpret_cast<T*>(vsapi->getWritePtr(dst, 0));
        T* VS_RESTRICT dstpG = reinterpret_cast<T*>(vsapi->getWritePtr(dst, 1));
        T* VS_RESTRICT dstpB = reinterpret_cast<T*>(vsapi->getWritePtr(dst, 2));

        //change back from Interleaved
        int target_height = d->target_height;
        int target_width = d->target_width;

        for (int y = 0; y < target_height; y++)
        {
            for (int x = 0; x < target_width; x++)
            {
                const unsigned pos = (x + y * target_width) * 3;
                dstpB[x] = dstInterleaved[pos];
                dstpG[x] = dstInterleaved[pos + 1];
                dstpR[x] = dstInterleaved[pos + 2];
            }
            dstpB += dst_stride;
            dstpG += dst_stride;
            dstpR += dst_stride;
        }

        delete[] srcInterleaved;
        delete[] bufInterleaved;
        delete[] dstInterleaved;
    }
}

static void VS_CC AreaInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi) {
    AreaData* d = static_cast<AreaData*>(*instanceData);
    VSVideoInfo dst_vi = (VSVideoInfo) * (d->vi);
    dst_vi.width = d->target_width;
    dst_vi.height = d->target_height;
    vsapi->setVideoInfo(&dst_vi, 1, node);
}

static const VSFrameRef* VS_CC AreaGetFrame(int n, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    const AreaData* d = static_cast<const AreaData*>(*instanceData);

    if (activationReason == arInitial)
    {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady)
    {
        const VSFrameRef* src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFormat* fi = d->vi->format;
        VSFrameRef* dst = vsapi->newVideoFrame(fi, d->target_width, d->target_height, src, core);
        VSFrameRef* buf = vsapi->newVideoFrame(fi, d->target_width, d->vi->height, src, core);

        if (fi->bytesPerSample == 1)
            process<uint8_t>(src, dst, buf, d, vsapi);
        else if (fi->bytesPerSample == 2)
            process<uint16_t>(src, dst, buf, d, vsapi);
        else
            process<float>(src, dst, buf, d, vsapi);

        vsapi->freeFrame(src);
        vsapi->freeFrame(buf);
        return dst;
    }

    return nullptr;
}

static void VS_CC AreaFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    AreaData* d = static_cast<AreaData*>(instanceData);
    vsapi->freeNode(d->node);

    if (d->vi->format->colorFamily == cmRGB && d->vi->format->bytesPerSample <= 2)
    {
        delete[] d->linear_LUT;
        delete[] d->gamma_LUT;
    }

    delete d;
}

static void VS_CC AreaCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    std::unique_ptr<AreaData> d = std::make_unique<AreaData>();
    int err;

    d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->vi = vsapi->getVideoInfo(d->node);

    d->target_width = int64ToIntS(vsapi->propGetInt(in, "width", 0, &err));
    d->target_height = int64ToIntS(vsapi->propGetInt(in, "height", 0, &err));

    double gamma = vsapi->propGetFloat(in, "gamma", 0, &err);
    if (err)
        gamma = 2.2;

    try
    {
        if (!isConstantFormat(d->vi) ||
            (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw std::string{ "Only constant format 8-16 bits integer and 32 bits float input supported." };

        if (d->target_width < 1 || d->target_height < 1)
            throw std::string{ "Target width/height must be 1 or higher." };

        if ((d->target_width & 1) || (d->target_height & 1))
            throw std::string{ "Target width and height requires mod 2." };

        if (d->vi->width < d->target_width || d->vi->height < d->target_height)
            throw std::string{ "This filter is only for downscale." };

        if (gamma <= 0)
            throw std::string{ "Gamma must be greater than 0." };
    }
    catch (const std::string& error)
    {
        vsapi->setError(out, ("AreaResize: " + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    if (d->vi->format->colorFamily == cmRGB)
    {
        // for 8bit RGB
        if (d->vi->format->bytesPerSample == 1)
        {
            int peak = (1 << d->vi->format->bitsPerSample) - 1;

            double* linear_LUT = new (std::nothrow) double[peak + 1];
            double* gamma_LUT = new (std::nothrow) double[RGB_PIXEL_RANGE_EXTENDED];

            for (int i = 0; i < peak + 1; i++)
            {
                linear_LUT[i] = pow(((double)i / peak), gamma) * peak;
                gamma_LUT[i] = pow((((double)i / 100.0) / peak), (1.0 / gamma)) * peak;
            }

            for (int i = peak + 1; i < RGB_PIXEL_RANGE_EXTENDED; i++)
                gamma_LUT[i] = pow((((double)i / 100.0) / peak), (1.0 / gamma)) * peak;

            d->linear_LUT = linear_LUT;
            d->gamma_LUT = gamma_LUT;
        }

        // for 9~16bit RGB
        else if (d->vi->format->bytesPerSample == 2)
        {
            int peak = (1 << d->vi->format->bitsPerSample) - 1;

            double* linear_LUT = new (std::nothrow) double[peak + 1];
            double* gamma_LUT = new (std::nothrow) double[peak + 1];

            for (int i = 0; i < peak + 1; i++)
            {
                linear_LUT[i] = pow(((double)i / peak), gamma) * peak;
                gamma_LUT[i] = pow(((double)i / peak), (1.0 / gamma)) * peak;
            }

            d->linear_LUT = linear_LUT;
            d->gamma_LUT = gamma_LUT;
        }
    }

    vsapi->createFilter(in, out, "AreaResize", AreaInit, AreaGetFrame, AreaFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.vapoursynth.arearesize", "area", "area average downscaler plugin", VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("AreaResize",
        "clip:clip;"
        "width:int;"
        "height:int;"
        "gamma:float:opt",
        AreaCreate, 0, plugin);
}
