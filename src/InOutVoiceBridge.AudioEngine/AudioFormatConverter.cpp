#include "AudioFormatConverter.h"
#include <cmath>
#include <cstring>

bool AudioFormatConverter::FormatsMatch(const WAVEFORMATEX* a, const WAVEFORMATEX* b) {
    if (!a || !b) return false;
    return a->nSamplesPerSec == b->nSamplesPerSec &&
           a->nChannels == b->nChannels &&
           a->wBitsPerSample == b->wBitsPerSample &&
           a->wFormatTag == b->wFormatTag;
}

bool AudioFormatConverter::ConvertBuffer(
    const uint8_t* src, size_t srcBytes, const WAVEFORMATEX* srcFmt,
    uint8_t* dst, size_t dstBytes, const WAVEFORMATEX* dstFmt,
    size_t* bytesWritten)
{
    if (!src || !dst || !srcFmt || !dstFmt || !bytesWritten) return false;

    if (FormatsMatch(srcFmt, dstFmt)) {
        size_t toCopy = (srcBytes < dstBytes) ? srcBytes : dstBytes;
        memcpy(dst, src, toCopy);
        *bytesWritten = toCopy;
        return true;
    }

    if (srcFmt->nSamplesPerSec == dstFmt->nSamplesPerSec &&
        srcFmt->wBitsPerSample == 32 && dstFmt->wBitsPerSample == 32) {
        WORD srcCh = srcFmt->nChannels;
        WORD dstCh = dstFmt->nChannels;
        size_t srcFrameSize = srcCh * sizeof(float);
        size_t dstFrameSize = dstCh * sizeof(float);
        size_t numFrames = srcBytes / srcFrameSize;
        size_t maxFrames = dstBytes / dstFrameSize;
        if (numFrames > maxFrames) numFrames = maxFrames;

        const float* srcF = reinterpret_cast<const float*>(src);
        float* dstF = reinterpret_cast<float*>(dst);

        for (size_t f = 0; f < numFrames; f++) {
            if (srcCh == 1 && dstCh == 2) {
                float mono = srcF[f];
                dstF[f * 2] = mono;
                dstF[f * 2 + 1] = mono;
            } else if (srcCh == 2 && dstCh == 1) {
                dstF[f] = (srcF[f * 2] + srcF[f * 2 + 1]) * 0.5f;
            } else {
                WORD minCh = (srcCh < dstCh) ? srcCh : dstCh;
                for (WORD c = 0; c < minCh; c++) {
                    dstF[f * dstCh + c] = srcF[f * srcCh + c];
                }
                for (WORD c = minCh; c < dstCh; c++) {
                    dstF[f * dstCh + c] = 0.0f;
                }
            }
        }

        *bytesWritten = numFrames * dstFrameSize;
        return true;
    }

    size_t toCopy = (srcBytes < dstBytes) ? srcBytes : dstBytes;
    memcpy(dst, src, toCopy);
    *bytesWritten = toCopy;
    return true;
}

float AudioFormatConverter::CalculateRms(const float* samples, size_t count) {
    if (!samples || count == 0) return 0.0f;
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double s = static_cast<double>(samples[i]);
        sum += s * s;
    }
    return static_cast<float>(sqrt(sum / count));
}
