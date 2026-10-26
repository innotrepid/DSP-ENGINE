/**
 * macOS / iOS Core Audio backend (AudioUnit / AudioQueue style via RemoteIO).
 *
 * Compiles when DSP_HAS_COREAUDIO is defined (auto on Apple hosts).
 */

#include "audio/backends/backend.h"

#if defined(DSP_HAS_COREAUDIO) && (defined(__APPLE__) || defined(__MACH__))

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <atomic>
#include <cstring>
#include <mutex>

namespace dsp {

class CoreAudioBackend final : public IAudioBackend {
public:
    CoreAudioBackend() = default;
    ~CoreAudioBackend() override { close(); }

    DspError open(const BackendConfig& cfg) override {
        close();

        AudioComponentDescription desc{};
        desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
        desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
        desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;

        AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
        if (!comp) return DSP_ERR_DEVICE;

        OSStatus st = AudioComponentInstanceNew(comp, &unit_);
        if (st != noErr || !unit_) return DSP_ERR_DEVICE;

        // Enable output
        UInt32 flag = 1;
        st = AudioUnitSetProperty(unit_, kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output, 0, &flag, sizeof(flag));
        if (st != noErr) { close(); return DSP_ERR_DEVICE; }

        // Stream format: float32, interleaved, native endian
        AudioStreamBasicDescription asbd{};
        asbd.mSampleRate = cfg.sample_rate;
        asbd.mFormatID = kAudioFormatLinearPCM;
        asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        asbd.mBytesPerPacket = sizeof(float) * cfg.channels;
        asbd.mFramesPerPacket = 1;
        asbd.mBytesPerFrame = asbd.mBytesPerPacket;
        asbd.mChannelsPerFrame = cfg.channels;
        asbd.mBitsPerChannel = 32;

        st = AudioUnitSetProperty(unit_, kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input, 0, &asbd, sizeof(asbd));
        if (st != noErr) { close(); return DSP_ERR_DEVICE; }

        // Callback
        AURenderCallbackStruct cb{};
        cb.inputProc = &CoreAudioBackend::render_callback;
        cb.inputProcRefCon = this;
        st = AudioUnitSetProperty(unit_, kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input, 0, &cb, sizeof(cb));
        if (st != noErr) { close(); return DSP_ERR_DEVICE; }

        // Preferred buffer size (frames)
        UInt32 frames = static_cast<UInt32>(cfg.buffer_frames);
        st = AudioUnitSetProperty(unit_, kAudioUnitProperty_MaximumFramesPerSlice,
                                  kAudioUnitScope_Global, 0, &frames, sizeof(frames));
        // Non-fatal if this fails on some devices

        st = AudioUnitInitialize(unit_);
        if (st != noErr) { close(); return DSP_ERR_DEVICE; }

        actual_rate_ = static_cast<int>(cfg.sample_rate);
        actual_ch_ = cfg.channels;
        actual_frames_ = cfg.buffer_frames;
        opened_ = true;
        xruns_.store(0, std::memory_order_relaxed);
        return DSP_OK;
    }

    void close() override {
        stop();
        if (unit_) {
            AudioUnitUninitialize(unit_);
            AudioComponentInstanceDispose(unit_);
            unit_ = nullptr;
        }
        opened_ = false;
    }

    DspError start() override {
        if (!opened_ || !unit_) return DSP_ERR_DEVICE;
        if (running_.load(std::memory_order_acquire)) return DSP_ERR_ALREADY_RUNNING;
        OSStatus st = AudioOutputUnitStart(unit_);
        if (st != noErr) return DSP_ERR_DEVICE;
        running_.store(true, std::memory_order_release);
        return DSP_OK;
    }

    DspError stop() override {
        if (!unit_) return DSP_OK;
        if (!running_.load(std::memory_order_acquire)) return DSP_OK;
        AudioOutputUnitStop(unit_);
        running_.store(false, std::memory_order_release);
        return DSP_OK;
    }

    bool is_running() const override {
        return running_.load(std::memory_order_acquire);
    }

    int xrun_count() const override {
        return xruns_.load(std::memory_order_relaxed);
    }

    int actual_sample_rate() const override { return actual_rate_; }
    int actual_channels() const override { return actual_ch_; }
    int actual_buffer_frames() const override { return actual_frames_; }

    void set_render_callback(RenderCallback cb, void* user) override {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        callback_ = cb;
        user_ = user;
    }

private:
    static OSStatus render_callback(void* inRefCon,
                                    AudioUnitRenderActionFlags* /*ioActionFlags*/,
                                    const AudioTimeStamp* /*inTimeStamp*/,
                                    UInt32 /*inBusNumber*/,
                                    UInt32 inNumberFrames,
                                    AudioBufferList* ioData)
    {
        auto* self = static_cast<CoreAudioBackend*>(inRefCon);
        if (!ioData || ioData->mNumberBuffers == 0) return noErr;

        float* out = static_cast<float*>(ioData->mBuffers[0].mData);
        int frames = static_cast<int>(inNumberFrames);

        RenderCallback cb = nullptr;
        void* u = nullptr;
        {
            std::lock_guard<std::mutex> lock(self->cb_mutex_);
            cb = self->callback_;
            u = self->user_;
        }
        if (cb) {
            cb(u, out, frames);
        } else {
            std::memset(out, 0, frames * self->actual_ch_ * sizeof(float));
        }
        return noErr;
    }

    AudioUnit unit_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<int> xruns_{0};
    bool opened_ = false;
    int actual_rate_ = 0;
    int actual_ch_ = 0;
    int actual_frames_ = 0;

    RenderCallback callback_ = nullptr;
    void* user_ = nullptr;
    std::mutex cb_mutex_;
};

IAudioBackend* create_coreaudio_backend() {
    return new (std::nothrow) CoreAudioBackend();
}

} // namespace dsp

#else // !DSP_HAS_COREAUDIO

namespace dsp {
IAudioBackend* create_coreaudio_backend() { return nullptr; }
} // namespace dsp

#endif
