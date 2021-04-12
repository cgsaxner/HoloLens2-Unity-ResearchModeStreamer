#include "pch.h"

#define DBG_ENABLE_VERBOSE_LOGGING 0
#define DBG_ENABLE_INFO_LOGGING 1
#define DBG_ENABLE_ERROR_LOGGING 1

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Capture::Frames;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

const int VideoCameraStreamer::kImageWidth = 640;
const wchar_t VideoCameraStreamer::kSensorName[3] = L"PV";


IAsyncAction VideoCameraStreamer::InitializeAsync(
    const long long minDelta,
    const SpatialCoordinateSystem& coordSystem,
    std::wstring portName)
{
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugStringW(L"VideoCameraStreamer::InitializeAsync: Creating Streamer for Video Camera. \n");
#endif
    m_worldCoordSystem = coordSystem;
    m_portName = portName;
    m_minDelta = minDelta;

    m_streamingEnabled = true;

    winrt::Windows::Foundation::Collections::IVectorView<MediaFrameSourceGroup>
        mediaFrameSourceGroups{ co_await MediaFrameSourceGroup::FindAllAsync() };

    MediaFrameSourceGroup selectedSourceGroup = nullptr;
    MediaCaptureVideoProfile profile = nullptr;
    MediaCaptureVideoProfileMediaDescription desc = nullptr;
    std::vector<MediaFrameSourceInfo> selectedSourceInfos;

    // Find MediaFrameSourceGroup
    for (const MediaFrameSourceGroup& mediaFrameSourceGroup : mediaFrameSourceGroups)
    {
        auto knownProfiles = MediaCapture::FindKnownVideoProfiles(
            mediaFrameSourceGroup.Id(),
            KnownVideoProfile::VideoConferencing);

        for (const auto& knownProfile : knownProfiles)
        {
            for (auto knownDesc : knownProfile.SupportedRecordMediaDescription())
            {
#if DBG_ENABLE_VERBOSE_LOGGING
                wchar_t msgBuffer[500];
                swprintf_s(msgBuffer, L"Profile: Frame width = %i, Frame height = %i, Frame rate = %f \n",
                    knownDesc.Width(), knownDesc.Height(), knownDesc.FrameRate());
                OutputDebugStringW(msgBuffer);
#endif
                if ((knownDesc.Width() == kImageWidth)) // && (std::round(knownDesc.FrameRate()) == 15))
                {
                    profile = knownProfile;
                    desc = knownDesc;
                    selectedSourceGroup = mediaFrameSourceGroup;
                    break;
                }
            }
        }
    }

    winrt::check_bool(selectedSourceGroup != nullptr);

    for (auto sourceInfo : selectedSourceGroup.SourceInfos())
    {
        // Workaround since multiple Color sources can be found,
        // and not all of them are necessarily compatible with the selected video profile
        if (sourceInfo.SourceKind() == MediaFrameSourceKind::Color)
        {
            selectedSourceInfos.push_back(sourceInfo);
        }
    }
    winrt::check_bool(!selectedSourceInfos.empty());

    // Initialize a MediaCapture object
    MediaCaptureInitializationSettings settings;
    settings.VideoProfile(profile);
    settings.RecordMediaDescription(desc);
    settings.VideoDeviceId(selectedSourceGroup.Id());
    settings.StreamingCaptureMode(StreamingCaptureMode::Video);
    settings.MemoryPreference(MediaCaptureMemoryPreference::Cpu);
    settings.SharingMode(MediaCaptureSharingMode::ExclusiveControl);
    settings.SourceGroup(selectedSourceGroup);

    MediaCapture mediaCapture = MediaCapture();
    co_await mediaCapture.InitializeAsync(settings);

    MediaFrameSource selectedSource = nullptr;
    MediaFrameFormat preferredFormat = nullptr;

    for (MediaFrameSourceInfo sourceInfo : selectedSourceInfos)
    {
        auto tmpSource = mediaCapture.FrameSources().Lookup(sourceInfo.Id());
        for (MediaFrameFormat format : tmpSource.SupportedFormats())
        {
            if (format.VideoFormat().Width() == kImageWidth)
            {
                selectedSource = tmpSource;
                preferredFormat = format;
                break;
            }
        }
    }

    winrt::check_bool(preferredFormat != nullptr);

    co_await selectedSource.SetFormatAsync(preferredFormat);
    MediaFrameReader mediaFrameReader = co_await mediaCapture.CreateFrameReaderAsync(selectedSource);
    MediaFrameReaderStartStatus status = co_await mediaFrameReader.StartAsync();

    winrt::check_bool(status == MediaFrameReaderStartStatus::Success);

    StartServer();

    m_pStreamThread = new std::thread(CameraStreamThread, this);
    m_OnFrameArrivedRegistration = mediaFrameReader.FrameArrived({ this, &VideoCameraStreamer::OnFrameArrived });

#if DBG_ENABLE_INFO_LOGGING
    OutputDebugStringW(L"VideoCameraStreamer::InitializeAsync: Done. \n");
#endif
}

void VideoCameraStreamer::OnFrameArrived(
    const MediaFrameReader& sender,
    const MediaFrameArrivedEventArgs& args)
{
    if (MediaFrameReference frame = sender.TryAcquireLatestFrame())
    {
        std::lock_guard<std::shared_mutex> lock(m_frameMutex);
        m_latestFrame = frame;
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"VideoCameraStreamer::CameraUpdateThread: Updated frame.\n");
#endif
    }
}

IAsyncAction VideoCameraStreamer::StartServer()
{
    try
    {
        // The ConnectionReceived event is raised when connections are received.
        m_streamSocketListener.ConnectionReceived({ this, &VideoCameraStreamer::OnConnectionReceived });

        // Start listening for incoming TCP connections on the specified port. You can specify any port that's not currently in use.
        // Every protocol typically has a standard port number. For example, HTTP is typically 80, FTP is 20 and 21, etc.
        // For this example, we'll choose an arbitrary port number.
        co_await m_streamSocketListener.BindServiceNameAsync(m_portName);
        //m_streamSocketListener.Control().KeepAlive(true);

#if DBG_ENABLE_INFO_LOGGING       
        wchar_t msgBuffer[200];
        swprintf_s(msgBuffer, L"VideoCameraStreamer::StartServer: Server is listening at %ls \n",
            m_portName.c_str());
        OutputDebugStringW(msgBuffer);
#endif
    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"VideoCameraStreamer::StartServer: Failed to open listener with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }
}

void VideoCameraStreamer::OnConnectionReceived(
    StreamSocketListener /* sender */,
    StreamSocketListenerConnectionReceivedEventArgs args)
{
    try
    {
        m_streamSocket = args.Socket();
        m_writer = args.Socket().OutputStream();
        m_writer.UnicodeEncoding(UnicodeEncoding::Utf8);
        m_writer.ByteOrder(ByteOrder::LittleEndian);

        m_writeInProgress = false;
#if DBG_ENABLE_INFO_LOGGING
        OutputDebugStringW(L"VideoCameraStreamer::OnConnectionReceived: Received connection! \n");
#endif
    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"VideoCameraStreamer::StartServer: Failed to open listener with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }
}

void VideoCameraStreamer::CameraStreamThread(VideoCameraStreamer* pStreamer)
{
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugString(L"VideoCameraStreamer::CameraStreamThread: Starting streaming thread.\n");
#endif
    while (!pStreamer->m_fExit)
    {
        std::lock_guard<std::shared_mutex> reader_guard(pStreamer->m_frameMutex);
        if (pStreamer->m_latestFrame)
        {
            MediaFrameReference frame = pStreamer->m_latestFrame;
            long long timestamp = pStreamer->m_converter.RelativeTicksToAbsoluteTicks(
                HundredsOfNanoseconds(frame.SystemRelativeTime().Value().count())).count();
            if (timestamp != pStreamer->m_latestTimestamp)
            {
                long long delta = timestamp - pStreamer->m_latestTimestamp;
                if (delta > pStreamer->m_minDelta)
                {
                    pStreamer->m_latestTimestamp = timestamp;
                    pStreamer->SendFrame(frame, timestamp);
                    pStreamer->m_writeInProgress = false;
                }
            }
        }
    }
}

void VideoCameraStreamer::SendFrame(
    MediaFrameReference pFrame,
    long long pTimestamp)
{
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Received frame for sending!\n");
#endif
    if (!m_streamSocket || !m_writer)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(
            L"VideoCameraStreamer::SendFrame: No connection.\n");
#endif
        return;
    }
    if (!m_streamingEnabled)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"Streamer::SendFrame: Streaming disabled.\n");
#endif
        return;
    }

    // grab the frame info
    float fx = pFrame.VideoMediaFrame().CameraIntrinsics().FocalLength().x;
    float fy = pFrame.VideoMediaFrame().CameraIntrinsics().FocalLength().y;

    winrt::Windows::Foundation::Numerics::float4x4 PVtoWorldtransform;
    auto PVtoWorld =
        m_latestFrame.CoordinateSystem().TryGetTransformTo(m_worldCoordSystem);
    if (PVtoWorld)
    {
        PVtoWorldtransform = PVtoWorld.Value();
    }

    // grab the frame data
    SoftwareBitmap softwareBitmap = SoftwareBitmap::Convert(
        pFrame.VideoMediaFrame().SoftwareBitmap(), BitmapPixelFormat::Bgra8);

    int imageWidth = softwareBitmap.PixelWidth();
    int imageHeight = softwareBitmap.PixelHeight();

    int pixelStride = 4;
    int scaleFactor = 1;

    int rowStride = imageWidth * pixelStride;

    // Get bitmap buffer object of the frame
    BitmapBuffer bitmapBuffer = softwareBitmap.LockBuffer(BitmapBufferAccessMode::Read);

    // Get raw pointer to the buffer object
    uint32_t pixelBufferDataLength = 0;
    uint8_t* pixelBufferData;

    auto spMemoryBufferByteAccess{ bitmapBuffer.CreateReference()
        .as<::Windows::Foundation::IMemoryBufferByteAccess>() };

    try
    {
        spMemoryBufferByteAccess->
            GetBuffer(&pixelBufferData, &pixelBufferDataLength);
    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        winrt::hresult hr = ex.code(); // HRESULT_FROM_WIN32
        winrt::hstring message = ex.message();
        OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Failed to get buffer with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }

    std::vector<uint8_t> imageBufferAsVector;
    for (int row = 0; row < imageHeight; row += scaleFactor)
    {
        for (int col = 0; col < rowStride; col += scaleFactor * pixelStride)
        {
            for (int j = 0; j < pixelStride - 1; j++)
            {
                imageBufferAsVector.emplace_back(
                    pixelBufferData[row * rowStride + col + j]);
            }
        }
    }


    if (m_writeInProgress)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(
            L"VideoCameraStreamer::SendFrame: Write in progress.\n");
#endif
        return;
    }
    m_writeInProgress = true;
    try
    {
        int outImageWidth = imageWidth / scaleFactor;
        int outImageHeight = imageHeight / scaleFactor;

        // pixel stride is reduced by 1 since we skip alpha channel
        int outPixelStride = pixelStride - 1;
        int outRowStride = outImageWidth * outPixelStride;


        // Write header
        m_writer.WriteUInt64(pTimestamp);
        m_writer.WriteInt32(outImageWidth);
        m_writer.WriteInt32(outImageHeight);
        m_writer.WriteInt32(outPixelStride);
        m_writer.WriteInt32(outRowStride);
        m_writer.WriteSingle(fx);
        m_writer.WriteSingle(fy);

        WriteMatrix4x4(PVtoWorldtransform);

        m_writer.WriteBytes(imageBufferAsVector);

#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Trying to store writer...\n");
#endif
        m_writer.StoreAsync();
    }
    catch (winrt::hresult_error const& ex)
    {
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        if (webErrorStatus == SocketErrorStatus::ConnectionResetByPeer)
        {
            // the client disconnected!
            m_writer == nullptr;
            m_streamSocket == nullptr;
            m_writeInProgress = false;
        }
#if DBG_ENABLE_ERROR_LOGGING
        winrt::hstring message = ex.message();
        OutputDebugStringW(L"RMCameraStreamer::SendFrame: Sending failed with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif // DBG_ENABLE_ERROR_LOGGING
    }

    m_writeInProgress = false;

#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(
        L"VideoCameraStreamer::SendFrame: Frame sent!\n");
#endif

}

void VideoCameraStreamer::StreamingToggle()
{
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugStringW(L"VideoCameraStreamer::StreamingToggle: Received!\n");
#endif
    if (m_streamingEnabled)
    {
        m_streamingEnabled = false;
    }
    else if (!m_streamingEnabled)
    {
        m_streamingEnabled = true;
    }
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugStringW(L"VideoCameraStreamer::StreamingToggle: Done!\n");
#endif
}

void VideoCameraStreamer::WriteMatrix4x4(
    _In_ winrt::Windows::Foundation::Numerics::float4x4 matrix)
{
    m_writer.WriteSingle(matrix.m11);
    m_writer.WriteSingle(matrix.m12);
    m_writer.WriteSingle(matrix.m13);
    m_writer.WriteSingle(matrix.m14);

    m_writer.WriteSingle(matrix.m21);
    m_writer.WriteSingle(matrix.m22);
    m_writer.WriteSingle(matrix.m23);
    m_writer.WriteSingle(matrix.m24);

    m_writer.WriteSingle(matrix.m31);
    m_writer.WriteSingle(matrix.m32);
    m_writer.WriteSingle(matrix.m33);
    m_writer.WriteSingle(matrix.m34);

    m_writer.WriteSingle(matrix.m41);
    m_writer.WriteSingle(matrix.m42);
    m_writer.WriteSingle(matrix.m43);
    m_writer.WriteSingle(matrix.m44);
}
