/**
 * Lock-free single-producer single-consumer (SPSC) ring buffer
 * for float32 interleaved audio.
 *
 * Real-time safe for the reader and writer (no locks, no allocations
 * after construction). Capacity is fixed at creation.
 */
#ifndef DSP_RING_BUFFER_H
#define DSP_RING_BUFFER_H

#include <atomic>
#include <cstdint>
#include <cstddef>

namespace dsp {

class RingBuffer {
public:
    /**
     * @param capacity_frames  Number of frames the buffer can hold.
     * @param channels         Channel count (interleaved storage).
     */
    RingBuffer(size_t capacity_frames, int channels);
    ~RingBuffer();

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /** Write up to `frames` of interleaved data. Returns frames actually written. */
    size_t write(const float* data, size_t frames);

    /** Read up to `frames` of interleaved data. Returns frames actually read. */
    size_t read(float* data, size_t frames);

    size_t available_read() const;
    size_t available_write() const;
    size_t capacity() const { return capacity_; }
    int channels() const { return channels_; }

    void clear();

private:
    const size_t          capacity_;
    const int             channels_;
    float*                buffer_;
    std::atomic<size_t>   write_pos_;
    std::atomic<size_t>   read_pos_;
};

} // namespace dsp

#endif // DSP_RING_BUFFER_H
