#pragma once

#include <memory>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <VapourSynth.h>
#include <VSHelper.h>

#ifdef LGHOST_X86
#include "VCL2/vectorclass.h"
#endif

using unique_float = std::unique_ptr<float[], decltype(&vs_aligned_free)>;

struct OptionData final {
    int shift, intensity, startX, endX;
};

struct LGhostData final {
    VSNodeRef * node;
    const VSVideoInfo * vi;
    bool process[3];
    std::vector<OptionData> options[3][4];
    int peak;
    std::unordered_map<std::thread::id, unique_float> buffer;
    void (*filter)(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept;
};
