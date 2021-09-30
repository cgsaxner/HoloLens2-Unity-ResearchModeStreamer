#include "pch.h"

#define DBG_ENABLE_VERBOSE_LOGGING 0
#define DBG_ENABLE_INFO_LOGGING 1
#define DBG_ENABLE_ERROR_LOGGING 1


using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Perception;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Foundation::Numerics;

ResearchModeFrameStreamer::ResearchModeFrameStreamer(
    std::wstring portName,
    const GUID& guid,
    const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& coordSystem)
{
    m_portName = portName;
    m_worldCoordSystem = coordSystem;

    // Get GUID identifying the rigNode to
    // initialize the SpatialLocator
    SetLocator(guid);

    StartServer();
}


// https://docs.microsoft.com/en-us/windows/uwp/networking/sockets
winrt::Windows::Foundation::IAsyncAction ResearchModeFrameStreamer::StartServer()
{
    try
    {
        // The ConnectionReceived event is raised when connections are received.
        m_streamSocketListener.ConnectionReceived({ this, &ResearchModeFrameStreamer::OnConnectionReceived });

        // Start listening for incoming TCP connections on the specified port. You can specify any port that's not currently in use.
        // Every protocol typically has a standard port number. For example, HTTP is typically 80, FTP is 20 and 21, etc.
        // For this example, we'll choose an arbitrary port number.
        co_await m_streamSocketListener.BindServiceNameAsync(m_portName);
#if DBG_ENABLE_INFO_LOGGING
        wchar_t msgBuffer[200];
        swprintf_s(msgBuffer, L"ResearchModeFrameStreamer::StartServer: Server is listening at %ls. \n",
            m_portName.c_str());
        OutputDebugStringW(msgBuffer);
#endif // DBG_ENABLE_INFO_LOGGING

    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"ResearchModeFrameStreamer::StartServer: Failed to open listener with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }
}

void ResearchModeFrameStreamer::OnConnectionReceived(
    StreamSocketListener /* sender */,
    StreamSocketListenerConnectionReceivedEventArgs args)
{
    try
    {
        m_streamSocket = args.Socket();
        m_writer = DataWriter(args.Socket().OutputStream());
        m_writer.UnicodeEncoding(UnicodeEncoding::Utf8);
        m_writer.ByteOrder(ByteOrder::LittleEndian);

        m_writeInProgress = false;
        isConnected = true;
        //m_streamingEnabled = true;
#if DBG_ENABLE_INFO_LOGGING
        wchar_t msgBuffer[200];
        swprintf_s(msgBuffer, L"ResearchModeFrameStreamer::OnConnectionReceived: Received connection at %ls. \n",
            m_portName.c_str());
        OutputDebugStringW(msgBuffer);
#endif // DBG_ENABLE_INFO_LOGGING
    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"ResearchModeFrameStreamer::OnConnectionReceived: Failed to establish connection with error ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }

}

void ResearchModeFrameStreamer::Send(
    std::shared_ptr<IResearchModeSensorFrame> frame,
    ResearchModeSensorType pSensorType)
{
#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Received frame for sending!\n");
#endif

    if (!m_streamSocket || !m_writer)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: No connection.\n");
#endif
        return;
    }

    // grab the frame info
    ResearchModeSensorTimestamp rmTimestamp;
    winrt::check_hresult(frame->GetTimeStamp(&rmTimestamp));
    auto prevTimestamp = rmTimestamp.HostTicks;

    auto timestamp = PerceptionTimestampHelper::FromSystemRelativeTargetTime(HundredsOfNanoseconds(checkAndConvertUnsigned(prevTimestamp)));
    auto location = m_locator.TryLocateAtTimestamp(timestamp, m_worldCoordSystem);
    if (!location)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Can't locate frame.\n");
#endif
        return;
    }
    const float4x4 rig2worldTransform = make_float4x4_from_quaternion(location.Orientation()) * make_float4x4_translation(location.Position());
    auto absoluteTimestamp = m_converter.RelativeTicksToAbsoluteTicks(HundredsOfNanoseconds((long long)prevTimestamp)).count();

    // grab the frame data
    ResearchModeSensorResolution resolution;
    IResearchModeSensorDepthFrame* pDepthFrame = nullptr;
    size_t outBufferCount;
    const UINT16* pDepth = nullptr;

    // invalidation value for AHAT 
    USHORT maxValue = 4090;

    frame->GetResolution(&resolution);
    HRESULT hr = frame->QueryInterface(IID_PPV_ARGS(&pDepthFrame));

    if (!pDepthFrame || !SUCCEEDED(hr))
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Failed to grab depth frame.\n");
#endif
        return;
    }

    std::shared_ptr<IResearchModeSensorDepthFrame> spDepthFrame(pDepthFrame, [](IResearchModeSensorDepthFrame* sf) { sf->Release(); });

    int imageWidth = resolution.Width;
    int imageHeight = resolution.Height;
    int pixelStride = resolution.BytesPerPixel;

    int rowStride = imageWidth * pixelStride;

    hr = spDepthFrame->GetBuffer(&pDepth, &outBufferCount);
    std::vector<BYTE> depthByteData;
    depthByteData.reserve(outBufferCount * sizeof(UINT16));

    // validate depth & append to vector
    for (size_t i = 0; i < outBufferCount; ++i)
    {
        // use a different invalidation condition for Long Throw and AHAT 
        const bool invalid = (pDepth[i] >= maxValue);
        UINT16 d;
        if (invalid)
        {
            d = 0;
        }
        else
        {
            d = pDepth[i];
        }
        depthByteData.push_back((BYTE)(d >> 8));
        depthByteData.push_back((BYTE)d);
    }

    if (m_writeInProgress)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Write already in progress.\n");
#endif
        return;
    }

    m_writeInProgress = true;

    try
    {
        // Write header
        m_writer.WriteUInt64(absoluteTimestamp);
        m_writer.WriteInt32(imageWidth);
        m_writer.WriteInt32(imageHeight);
        m_writer.WriteInt32(pixelStride);
        m_writer.WriteInt32(rowStride);

        WriteMatrix4x4(rig2worldTransform);

        m_writer.WriteBytes(depthByteData);

#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Trying to store writer...\n");
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
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Sending failed with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif // DBG_ENABLE_ERROR_LOGGING
    }

    m_writeInProgress = false;

#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Frame sent!\n");
#endif
}


void ResearchModeFrameStreamer::WriteMatrix4x4(
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

void ResearchModeFrameStreamer::SetLocator(const GUID& guid)
{
    m_locator = Preview::SpatialGraphInteropPreview::CreateLocatorForNode(guid);
}
