#pragma once

#include <atomic>
#include <cstring>
#include <algorithm>

class RingBuffer {
public:
    RingBuffer(size_t capacityBytes)
        : m_capacity(capacityBytes)
        , m_buffer(new uint8_t[capacityBytes])
        , m_writePos(0)
        , m_readPos(0)
    {
        memset(m_buffer, 0, capacityBytes);
    }

    ~RingBuffer() {
        delete[] m_buffer;
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    size_t Write(const uint8_t* data, size_t bytes) {
        size_t avail = AvailableWrite();
        size_t toWrite = (std::min)(bytes, avail);
        if (toWrite == 0) return 0;

        size_t wp = m_writePos.load(std::memory_order_relaxed);
        size_t firstChunk = (std::min)(toWrite, m_capacity - wp);
        memcpy(m_buffer + wp, data, firstChunk);

        if (toWrite > firstChunk) {
            memcpy(m_buffer, data + firstChunk, toWrite - firstChunk);
        }

        m_writePos.store((wp + toWrite) % m_capacity, std::memory_order_release);
        return toWrite;
    }

    size_t Read(uint8_t* data, size_t bytes) {
        size_t avail = AvailableRead();
        size_t toRead = (std::min)(bytes, avail);
        if (toRead == 0) return 0;

        size_t rp = m_readPos.load(std::memory_order_relaxed);
        size_t firstChunk = (std::min)(toRead, m_capacity - rp);
        memcpy(data, m_buffer + rp, firstChunk);

        if (toRead > firstChunk) {
            memcpy(data + firstChunk, m_buffer, toRead - firstChunk);
        }

        m_readPos.store((rp + toRead) % m_capacity, std::memory_order_release);
        return toRead;
    }

    size_t AvailableRead() const {
        size_t wp = m_writePos.load(std::memory_order_acquire);
        size_t rp = m_readPos.load(std::memory_order_acquire);
        if (wp >= rp)
            return wp - rp;
        return m_capacity - rp + wp;
    }

    size_t AvailableWrite() const {
        return m_capacity - AvailableRead() - 1;
    }

    void Reset() {
        m_writePos.store(0, std::memory_order_release);
        m_readPos.store(0, std::memory_order_release);
    }

private:
    size_t m_capacity;
    uint8_t* m_buffer;
    std::atomic<size_t> m_writePos;
    std::atomic<size_t> m_readPos;
};
