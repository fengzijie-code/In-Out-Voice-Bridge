#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <cstdint>
#include <cstddef>

class AudioFormatConverter {
public:
    static bool FormatsMatch(const WAVEFORMATEX* a, const WAVEFORMATEX* b);
    static bool ConvertBuffer(
        const uint8_t* src, size_t srcBytes, const WAVEFORMATEX* srcFmt,
        uint8_t* dst, size_t dstBytes, const WAVEFORMATEX* dstFmt,
        size_t* bytesWritten);

    static float CalculateRms(const float* samples, size_t count);
};
