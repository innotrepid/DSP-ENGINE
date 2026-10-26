/**
 * Windows WASAPI backend — shared and exclusive modes.
 *
 * Compiles when DSP_HAS_WASAPI is defined (auto on native Windows hosts).
 * Links: ole32, avrt.
 */

#include "audio/backends/backend.h"

#if defined(DSP_HAS_WASAPI) && defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

namespace dsp {

class WasapiBackend final : public IAudioBackend {
public:
    WasapiBackend() = default;
    ~WasapiBackend() override { close(); }

    DspError open(const BackendConfig& cfg) override {
        close();

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        // S_FALSE is ok if already initialized
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return DSP_ERR_DEVICE;

        IMMDeviceEnumerator* enumerator = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
        if (FAILED(hr) || !enumerator) return DSP_ERR_DEVICE;

        IMMDevice* device = nullptr;
        if (cfg.device_id > 0) {
            // Simple: still use default for now; device_id can be extended later
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        } else {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        }
        enumerator->Release();
        if (FAILED(hr) || !device) return DSP_ERR_DEVICE;

        IAudioClient* client = nullptr;
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
        device->Release();
        if (FAILED(hr) || !client) return DSP_ERR_DEVICE;

        WAVEFORMATEX* mix = nullptr;
        hr = client->GetMixFormat(&mix);
        if (FAILED(hr) || !mix) {
            client->Release();
            return DSP_ERR_DEVICE;
        }

        // Prefer float32 if possible; otherwise use mix format and convert later if needed
        WAVEFORMATEXTENSIBLE wfx{};
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels = static_cast<WORD>(cfg.channels);
        wfx.Format.nSamplesPerSec = cfg.sample_rate;
        wfx.Format.wBitsPerSample = 32;
        wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
        wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
        wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx.Samples.wValidBitsPerSample = 32;
        wfx.dwChannelMask = (cfg.channels == 1) ? SPEAKER_FRONT_CENTER : (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
        wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        REFERENCE_TIME duration = (REFERENCE_TIME)(10000000.0 * cfg.buffer_frames / cfg.sample_rate);
        if (duration < 30000) duration = 30000; // ~3 ms minimum safety

        DWORD flags = 0;
        if (cfg.exclusive) {
            flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
            hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, flags, duration, duration,
                                    (WAVEFORMATEX*)&wfx, nullptr);
            if (FAILED(hr)) {
                // Fallback to shared
                flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
                hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, duration, 0,
                                        (WAVEFORMATEX*)&wfx, nullptr);
            }
        } else {
            flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, duration, 0,
                                    (WAVEFORMATEX*)&wfx, nullptr);
        }

        CoTaskMemFree(mix);

        if (FAILED(hr)) {
            client->Release();
            return DSP_ERR_DEVICE;
        }

        // Event for callback-driven rendering
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event_) {
            client->Release();
            return DSP_ERR_DEVICE;
        }
        hr = client->SetEventHandle(event_);
        if (FAILED(hr)) {
            CloseHandle(event_);
            event_ = nullptr;
            client->Release();
            return DSP_ERR_DEVICE;
        }

        IAudioRenderClient* render = nullptr;
        hr = client->GetService(__uuidof(IAudioRenderClient), (void**)&render);
        if (FAILED(hr) || !render) {
            CloseHandle(event_);
            event_ = nullptr;
            client->Release();
            return DSP_ERR_DEVICE;
        }

        UINT32 bufferFrames = 0;
        client->GetBufferSize(&bufferFrames);

        client_ = client;
        render_ = render;
        actual_rate_ = cfg.sample_rate;
        actual_ch_ = cfg.channels;
        actual_frames_ = static_cast<int>(bufferFrames);
        opened_ = true;
        xruns_.store(0, std::memory_order_relaxed);
        return DSP_OK;
    }

    void close() override {
        stop();
        if (render_) { render_->Release(); render_ = nullptr; }
        if (client_) { client_->Release(); client_ = nullptr; }
        if (event_) { CloseHandle(event_); event_ = nullptr; }
        opened_ = false;
    }

    DspError start() override {
        if (!opened_ || !client_) return DSP_ERR_DEVICE;
        if (running_.load(std::memory_order_acquire)) return DSP_ERR_ALREADY_RUNNING;

        // Pre-fill silence
        UINT32 padding = 0;
        client_->GetCurrentPadding(&padding);
        UINT32 avail = actual_frames_ - padding;
        if (avail > 0) {
            BYTE* data = nullptr;
            if (SUCCEEDED(render_->GetBuffer(avail, &data)) && data) {
                std::memset(data, 0, avail * actual_ch_ * sizeof(float));
                render_->ReleaseBuffer(avail, 0);
            }
        }

        HRESULT hr = client_->Start();
        if (FAILED(hr)) return DSP_ERR_DEVICE;

        running_.store(true, std::memory_order_release);
        thread_ = std::thread(&WasapiBackend::render_thread, this);
        return DSP_OK;
    }

    DspError stop() override {
        if (!running_.load(std::memory_order_acquire)) return DSP_OK;
        running_.store(false, std::memory_order_release);
        if (event_) SetEvent(event_);
        if (thread_.joinable()) thread_.join();
        if (client_) client_->Stop();
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
    void render_thread() {
        // Raise thread priority for audio
        DWORD taskIndex = 0;
        HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

        while (running_.load(std::memory_order_acquire)) {
            DWORD wait = WaitForSingleObject(event_, 100);
            if (wait != WAIT_OBJECT_0) continue;

            UINT32 padding = 0;
            if (FAILED(client_->GetCurrentPadding(&padding))) {
                xruns_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            UINT32 frames = actual_frames_ - padding;
            if (frames == 0) continue;

            BYTE* data = nullptr;
            HRESULT hr = render_->GetBuffer(frames, &data);
            if (FAILED(hr) || !data) {
                xruns_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            float* out = reinterpret_cast<float*>(data);
            RenderCallback cb = nullptr;
            void* u = nullptr;
            {
                std::lock_guard<std::mutex> lock(cb_mutex_);
                cb = callback_;
                u = user_;
            }
            if (cb) {
                cb(u, out, static_cast<int>(frames));
            } else {
                std::memset(out, 0, frames * actual_ch_ * sizeof(float));
            }

            render_->ReleaseBuffer(frames, 0);
        }

        if (task) AvRevertMmThreadCharacteristics(task);
    }

    IAudioClient* client_ = nullptr;
    IAudioRenderClient* render_ = nullptr;
    HANDLE event_ = nullptr;
    std::thread thread_;
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

IAudioBackend* create_wasapi_backend() {
    return new (std::nothrow) WasapiBackend();
}

} // namespace dsp

#else // !DSP_HAS_WASAPI || !_WIN32

namespace dsp {
IAudioBackend* create_wasapi_backend() { return nullptr; }
} // namespace dsp

#endif
