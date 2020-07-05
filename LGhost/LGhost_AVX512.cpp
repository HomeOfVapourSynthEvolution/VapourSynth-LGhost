#ifdef LGHOST_X86
#include "LGhost.h"

template<typename pixel_t>
void filter_avx512(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept {
    using var_t = std::conditional_t<std::is_integral_v<pixel_t>, int, float>;
    using vec_t = std::conditional_t<std::is_integral_v<pixel_t>, Vec16i, Vec16f>;

    const auto threadId = std::this_thread::get_id();
    var_t * _buffer = reinterpret_cast<var_t *>(d->buffer.at(threadId).get());

    auto load = [](const pixel_t * srcp) noexcept {
        if constexpr (std::is_same_v<pixel_t, uint8_t>)
            return vec_t().load_16uc(srcp);
        else if constexpr (std::is_same_v<pixel_t, uint16_t>)
            return vec_t().load_16us(srcp);
        else
            return vec_t().load(srcp);
    };

    auto store = [&](const vec_t srcp, const vec_t buffer, pixel_t * dstp) noexcept {
        if constexpr (std::is_same_v<pixel_t, uint8_t>) {
            const auto result = compress_saturated_s2u(compress_saturated(srcp + (buffer >> 7), zero_si512()), zero_si512()).get_low().get_low();
            result.store_nt(dstp);
        } else if constexpr (std::is_same_v<pixel_t, uint16_t>) {
            const auto result = compress_saturated_s2u(srcp + (buffer >> 7), zero_si512()).get_low();
            min(result, d->peak).store_nt(dstp);
        } else {
            const auto result = mul_add(buffer, 1.0f / 128.0f, srcp);
            result.store_nt(dstp);
        }
    };

    for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
        if (d->process[plane]) {
            const int width = vsapi->getFrameWidth(src, plane);
            const int height = vsapi->getFrameHeight(src, plane);
            const int stride = vsapi->getStride(src, plane) / sizeof(pixel_t);
            const pixel_t * _srcp = reinterpret_cast<const pixel_t *>(vsapi->getReadPtr(src, plane));
            pixel_t * dstp = reinterpret_cast<pixel_t *>(vsapi->getWritePtr(dst, plane));

            const int regularPart = (width - 1) & ~(vec_t().size() - 1);

            for (int y = 0; y < height; y++) {
                memset(_buffer, 0, width * sizeof(var_t));

                for (auto && it : d->options[plane][0]) {
                    const vec_t intensity = static_cast<var_t>(it.intensity);
                    int x = 0;

                    if (it.endX - it.startX >= vec_t().size()) {
                        for (x = it.startX; x - it.shift + 1 <= regularPart; x += vec_t().size()) {
                            const vec_t buffer = vec_t().load(_buffer + x);
                            const vec_t result = buffer + (load(_srcp + x - it.shift + 1) - load(_srcp + x - it.shift)) * intensity;
                            result.store(_buffer + x);
                        }
                    }

                    for (; x < it.endX; x++)
                        _buffer[x] += (_srcp[x - it.shift + 1] - _srcp[x - it.shift]) * it.intensity;
                }

                for (auto && it : d->options[plane][1]) {
                    const vec_t intensity = static_cast<var_t>(it.intensity);
                    int x = 0;

                    if (it.endX - it.startX >= vec_t().size()) {
                        for (x = it.startX; x - it.shift <= regularPart; x += vec_t().size()) {
                            const vec_t buffer = vec_t().load(_buffer + x);
                            const vec_t result = buffer + load(_srcp + x - it.shift) * intensity;
                            result.store(_buffer + x);
                        }
                    }

                    for (; x < it.endX; x++)
                        _buffer[x] += _srcp[x - it.shift] * it.intensity;
                }

                for (auto && it : d->options[plane][2]) {
                    const vec_t intensity = static_cast<var_t>(it.intensity);
                    int x = 0;

                    if (it.endX - it.startX >= vec_t().size()) {
                        for (x = it.startX; x - it.shift + 1 <= regularPart; x += vec_t().size()) {
                            const vec_t buffer = vec_t().load(_buffer + x);
                            const vec_t tempEdge = load(_srcp + x - it.shift + 1) - load(_srcp + x - it.shift);
                            const vec_t result = select(tempEdge > 0, buffer + tempEdge * intensity, buffer);
                            result.store(_buffer + x);
                        }
                    }

                    for (; x < it.endX; x++)
                        if (const var_t tempEdge = _srcp[x - it.shift + 1] - _srcp[x - it.shift]; tempEdge > 0)
                            _buffer[x] += tempEdge * it.intensity;
                }

                for (auto && it : d->options[plane][3]) {
                    const vec_t intensity = static_cast<var_t>(it.intensity);
                    int x = 0;

                    if (it.endX - it.startX >= vec_t().size()) {
                        for (x = it.startX; x - it.shift + 1 <= regularPart; x += vec_t().size()) {
                            const vec_t buffer = vec_t().load(_buffer + x);
                            const vec_t tempEdge = load(_srcp + x - it.shift + 1) - load(_srcp + x - it.shift);
                            const vec_t result = select(tempEdge < 0, buffer + tempEdge * intensity, buffer);
                            result.store(_buffer + x);
                        }
                    }

                    for (; x < it.endX; x++)
                        if (const var_t tempEdge = _srcp[x - it.shift + 1] - _srcp[x - it.shift]; tempEdge < 0)
                            _buffer[x] += tempEdge * it.intensity;
                }

                for (int x = 0; x < width; x += vec_t().size()) {
                    const vec_t srcp = load(_srcp + x);
                    const vec_t buffer = vec_t().load_a(_buffer + x);
                    store(srcp, buffer, dstp + x);
                }

                _srcp += stride;
                dstp += stride;
            }
        }
    }
}

template void filter_avx512<uint8_t>(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept;
template void filter_avx512<uint16_t>(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept;
template void filter_avx512<float>(const VSFrameRef * src, VSFrameRef * dst, const LGhostData * const VS_RESTRICT d, const VSAPI * vsapi) noexcept;
#endif
