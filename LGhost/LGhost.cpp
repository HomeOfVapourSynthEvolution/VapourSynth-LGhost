// VapourSynth port by HolyWu
//
// LGhost.dll v0.3.01 Copyright(C) 2002, 2003 minamina
// Avisynth Plugin - Ghost Reduction (YUY2 and YV12 Only, Luminance Ghost Only)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <cstdlib>

#include <algorithm>
#include <string>

#include "LGhost.h"

using namespace std::literals;

#ifdef LGHOST_X86
template<typename pixel_t> extern void filter_sse2(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept;
template<typename pixel_t> extern void filter_avx2(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept;
template<typename pixel_t> extern void filter_avx512(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept;
#endif

template<typename pixel_t>
static void filter_c(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept {
    using var_t = std::conditional_t<std::is_integral_v<pixel_t>, int, float>;

    const auto threadId = std::this_thread::get_id();
    var_t * buffer = reinterpret_cast<var_t *>(d->buffer.at(threadId).get());

    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int stride = vsapi->getStride(src, plane) / sizeof(pixel_t);
            const pixel_t * srcp = reinterpret_cast<const pixel_t *>(vsapi->getReadPtr(src, plane));
            pixel_t * VS_RESTRICT dstp = reinterpret_cast<pixel_t *>(vsapi->getWritePtr(dst, plane));

            for (int y = 0; y < height; y++) {
                memset(buffer, 0, width * sizeof(var_t));

                for (auto && it : d->options[plane][0])
                    for (int x = it.startX; x < it.endX; x++)
                        buffer[x] += (srcp[x - it.shift + 1] - srcp[x - it.shift]) * it.intensity;

                for (auto && it : d->options[plane][1])
                    for (int x = it.startX; x < it.endX; x++)
                        buffer[x] += srcp[x - it.shift] * it.intensity;

                for (auto && it : d->options[plane][2])
                    for (int x = it.startX; x < it.endX; x++)
                        if (const var_t tempEdge = srcp[x - it.shift + 1] - srcp[x - it.shift]; tempEdge > 0)
                            buffer[x] += tempEdge * it.intensity;

                for (auto && it : d->options[plane][3])
                    for (int x = it.startX; x < it.endX; x++)
                        if (const var_t tempEdge = srcp[x - it.shift + 1] - srcp[x - it.shift]; tempEdge < 0)
                            buffer[x] += tempEdge * it.intensity;

                for (int x = 0; x < width; x++) {
                    if constexpr (std::is_integral_v<pixel_t>)
                        dstp[x] = std::clamp(srcp[x] + (buffer[x] >> 7), 0, d->peak);
                    else
                        dstp[x] = srcp[x] + (buffer[x] * (1.0f / 128.0f));
                }

                srcp += stride;
                dstp += stride;
            }
        }
    }
}

static void VS_CC lghostInit(VSMap * in, VSMap * out, void ** instanceData, VSNode * node, VSCore * core, const VSAPI * vsapi) {
    LGhostData * d = static_cast<LGhostData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef * VS_CC lghostGetFrame(int n, int activationReason, void ** instanceData, void ** frameData, VSFrameContext * frameCtx, VSCore * core, const VSAPI * vsapi) {
    LGhostData * d = static_cast<LGhostData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        try {
            auto threadId = std::this_thread::get_id();

            if (!d->buffer.count(threadId)) {
                float * buffer = vs_aligned_malloc<float>((d->vi->width + 15) * sizeof(float), 64);
                if (!buffer)
                    throw "malloc failure (buffer)";
                d->buffer.emplace(threadId, unique_float{ buffer, vs_aligned_free });
            }
        } catch (const char * error) {
            vsapi->setFilterError(("LGhost: "s + error).c_str(), frameCtx);
            return nullptr;
        }

        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef * fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        const int pl[] = { 0, 1, 2 };
        VSFrameRef * dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        d->filter(src, dst, d, vsapi);

        vsapi->freeFrame(src);
        return dst;
    }

    return nullptr;
}

static void VS_CC lghostFree(void * instanceData, VSCore * core, const VSAPI * vsapi) {
    LGhostData * d = static_cast<LGhostData *>(instanceData);
    vsapi->freeNode(d->node);
    delete d;
}

static void VS_CC lghostCreate(const VSMap * in, VSMap * out, void * userData, VSCore * core, const VSAPI * vsapi) {
    std::unique_ptr<LGhostData> d = std::make_unique<LGhostData>();

    try {
        d->node = vsapi->propGetNode(in, "clip", 0, nullptr);
        d->vi = vsapi->getVideoInfo(d->node);
        int err;

        if (!isConstantFormat(d->vi) ||
            (d->vi->format->sampleType == stInteger && d->vi->format->bitsPerSample > 16) ||
            (d->vi->format->sampleType == stFloat && d->vi->format->bitsPerSample != 32))
            throw "only constant format 8-16 bit integer and 32 bit float input supported";

        const int64_t * modeArray = vsapi->propGetIntArray(in, "mode", nullptr);
        const int64_t * shiftArray = vsapi->propGetIntArray(in, "shift", nullptr);
        const int64_t * intensityArray = vsapi->propGetIntArray(in, "intensity", nullptr);

        const int numMode = vsapi->propNumElements(in, "mode");
        const int numShift = vsapi->propNumElements(in, "shift");
        const int numIntensity = vsapi->propNumElements(in, "intensity");

        {
            const int m = vsapi->propNumElements(in, "planes");

            if (m <= 0) {
                for (int i = 0; i < 3; i++) {
                    d->process[i] = true;
                    if (i == 0 && d->vi->format->colorFamily != cmRGB)
                        break;
                }
            }

            for (int i = 0; i < m; i++) {
                const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

                if (n < 0 || n >= d->vi->format->numPlanes)
                    throw "plane index out of range";

                if (d->process[n])
                    throw "plane specified twice";

                d->process[n] = true;
            }
        }

        const int opt = int64ToIntS(vsapi->propGetInt(in, "opt", 0, &err));

        if (numMode != numShift || numMode != numIntensity)
            throw "number of elements in mode, shift and intensity must equal";

        for (int i = 0; i < numMode; i++) {
            const int mode = int64ToIntS(modeArray[i]);
            const int intensity = int64ToIntS(intensityArray[i]);

            if (mode < 1 || mode > 4)
                throw "mode must be 1, 2, 3, or 4";

            if (intensity == 0 || intensity < -128 || intensity > 127)
                throw "intensity must not be 0 and must be between -128 and 127 (inclusive)";

            for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
                if (d->process[plane]) {
                    int shift = int64ToIntS(shiftArray[i]);
                    int width = d->vi->width >> (plane ? d->vi->format->subSamplingW : 0);

                    if (std::abs(shift) >= width)
                        throw "abs(shift) must be less than plane's width";

                    if (mode != 2) {
                        if (shift == 0)
                            throw "shift must not be 0 for mode 1, 3, 4";

                        if (shift < 0) {
                            shift++;
                            width--;
                        }
                    }

                    d->options[plane][mode - 1].emplace_back(OptionData{ shift, intensity, std::max(shift, 0), std::min(width + shift, width) });
                }
            }
        }

        if (opt < 0 || opt > 4)
            throw "opt must be 0, 1, 2, 3, or 4";

        {
            if (d->vi->format->bytesPerSample == 1)
                d->filter = filter_c<uint8_t>;
            else if (d->vi->format->bytesPerSample == 2)
                d->filter = filter_c<uint16_t>;
            else
                d->filter = filter_c<float>;

#ifdef LGHOST_X86
            const int iset = instrset_detect();
            if ((opt == 0 && iset >= 10) || opt == 4) {
                if (d->vi->format->bytesPerSample == 1)
                    d->filter = filter_avx512<uint8_t>;
                else if (d->vi->format->bytesPerSample == 2)
                    d->filter = filter_avx512<uint16_t>;
                else
                    d->filter = filter_avx512<float>;
            } else if ((opt == 0 && iset >= 8) || opt == 3) {
                if (d->vi->format->bytesPerSample == 1)
                    d->filter = filter_avx2<uint8_t>;
                else if (d->vi->format->bytesPerSample == 2)
                    d->filter = filter_avx2<uint16_t>;
                else
                    d->filter = filter_avx2<float>;
            } else if ((opt == 0 && iset >= 2) || opt == 2) {
                if (d->vi->format->bytesPerSample == 1)
                    d->filter = filter_sse2<uint8_t>;
                else if (d->vi->format->bytesPerSample == 2)
                    d->filter = filter_sse2<uint16_t>;
                else
                    d->filter = filter_sse2<float>;
            }
#endif
        }

        if (d->vi->format->sampleType == stInteger)
            d->peak = (1 << d->vi->format->bitsPerSample) - 1;

        d->buffer.reserve(vsapi->getCoreInfo(core)->numThreads);
    } catch (const char * error) {
        vsapi->setError(out, ("LGhost: "s + error).c_str());
        vsapi->freeNode(d->node);
        return;
    }

    vsapi->createFilter(in, out, "LGhost", lghostInit, lghostGetFrame, lghostFree, fmParallel, 0, d.release(), core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin * plugin) {
    configFunc("com.holywu.lghost", "lghost", "Ghost Reduction", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("LGhost",
                 "clip:clip;"
                 "mode:int[];"
                 "shift:int[];"
                 "intensity:int[];"
                 "planes:int[]:opt;"
                 "opt:int:opt;",
                 lghostCreate, nullptr, plugin);
}
