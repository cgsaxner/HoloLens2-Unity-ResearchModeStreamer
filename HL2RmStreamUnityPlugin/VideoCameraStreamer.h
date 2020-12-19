#pragma once

class VideoCameraStreamer
{
public:
    VideoCameraStreamer()
    {
    }

    virtual ~VideoCameraStreamer()
    {
        m_fExit = true;
        m_pStreamThread->join();
    }

    winrt::Windows::Foundation::IAsyncAction InitializeAsync(
        const long long minDelta,
        const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& coordSystem,
        std::wstring portName);

    void StreamingToggle();

protected:
    void OnFrameArrived(const winrt::Windows::Media::Capture::Frames::MediaFrameReader& sender,
        const winrt::Windows::Media::Capture::Frames::MediaFrameArrivedEventArgs& args);

private:
    winrt::Windows::Foundation::IAsyncAction StartServer();

    void OnConnectionReceived(
        winrt::Windows::Networking::Sockets::StreamSocketListener /* sender */,
        winrt::Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs args);

    static void CameraStreamThread(VideoCameraStreamer* pProcessor);

    void SendFrame(
        winrt::Windows::Media::Capture::Frames::MediaFrameReference pFrame,
        long long pTimestamp);

    void WriteMatrix4x4(
        _In_ winrt::Windows::Foundation::Numerics::float4x4 matrix);

    std::shared_mutex m_frameMutex;
    long long m_latestTimestamp = 0;
    winrt::Windows::Media::Capture::Frames::MediaFrameReference m_latestFrame = nullptr;

    winrt::Windows::Media::Capture::Frames::MediaFrameReader m_mediaFrameReader = nullptr;
    winrt::event_token m_OnFrameArrivedRegistration;

    // streaming thread
    std::thread* m_pStreamThread = nullptr;
    bool m_fExit = false;

    bool m_streamingEnabled = true;

    TimeConverter m_converter;
    winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_worldCoordSystem = nullptr;

    winrt::Windows::Networking::Sockets::StreamSocketListener m_streamSocketListener;
    winrt::Windows::Networking::Sockets::StreamSocket m_streamSocket = nullptr;
    winrt::Windows::Storage::Streams::DataWriter m_writer;
    bool m_writeInProgress = false;

    std::wstring m_portName;
    // minDelta allows to enforce a certain time delay between frames
    // should be set in hundreds of nanoseconds (ms * 1e-4)
    long long m_minDelta;

    static const int kImageWidth;
    static const wchar_t kSensorName[3];
    
};

