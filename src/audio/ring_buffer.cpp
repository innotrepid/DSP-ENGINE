/**
 * Lock-free single-producer single-consumer ring buffer
 * (for future use between decoder thread and audio callback)
 */

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdlib>

namespace dsp {

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity_frames, int channels)
        : capacity_(capacity_frames)
        , channels_(channels)
        , buffer_(static_cast<float*>(std::aligned_alloc(64, capacity_frames * channels * sizeof(float))))
        , write_pos_(0)
        , read_pos_(0)
    {
        if (buffer_)
            std::memset(buffer_, 0, capacity_frames * channels * sizeof(float));
    }

    ~RingBuffer()
    {
        std::free(buffer_);
    }

    // Returns number of frames written
    size_t write(const float* data, size_t frames)
    {
        if (!buffer_ || frames == 0) return 0;

        size_t available = capacity_ - (write_pos_.load(std::memory_order_relaxed) -
                                        read_pos_.load(std::memory_order_acquire));
        if (available > capacity_) available = 0; // wrap safety

        size_t to_write = frames < available ? frames : available;
        if (to_write == 0) return 0;

        size_t w = write_pos_.load(std::memory_order_relaxed) % capacity_;
        size_t first = capacity_ - w;
        if (first > to_write) first = to_write;

        const size_t sample_stride = channels_;
        std::memcpy(buffer_ + w * sample_stride, data, first * sample_stride * sizeof(float));
        if (to_write > first)
            std::memcpy(buffer_, data + first * sample_stride,
                        (to_write - first) * sample_stride * sizeof(float));

        write_pos_.fetch_add(to_write, std::memory_order_release);
        return to_write;
    }

    // Returns number of frames read
    size_t read(float* data, size_t frames)
    {
        if (!buffer_ || frames == 0) return 0;

        size_t available = write_pos_.load(std::memory_order_acquire) -
                           read_pos_.load(std::memory_order_relaxed);
        size_t to_read = frames < available ? frames : available;
        if (to_read == 0) return 0;

        size_t r = read_pos_.load(std::memory_order_relaxed) % capacity_;
        size_t first = capacity_ - r;
        if (first > to_read) first = to_read;

        const size_t sample_stride = channels_;
        std::memcpy(data, buffer_ + r * sample_stride, first * sample_stride * sizeof(float));
        if (to_read > first)
            std::memcpy(data + first * sample_stride, buffer_,
                        (to_read - first) * sample_stride * sizeof(float));

        read_pos_.fetch_add(to_read, std::memory_order_release);
        return to_read;
    }

    size_t available_read() const
    {
        return write_pos_.load(std::memory_order_acquire) -
               read_pos_.load(std::memory_order_relaxed);
    }

    size_t available_write() const
    {
        size_t used = write_pos_.load(std::memory_order_relaxed) -
                      read_pos_.load(std::memory_order_acquire);
        return capacity_ - used;
    }

private:
    const size_t          capacity_;
    const int             channels_;
    float*                buffer_;
    std::atomic<size_t>   write_pos_;
    std::atomic<size_t>   read_pos_;
};

} // namespace dsp
