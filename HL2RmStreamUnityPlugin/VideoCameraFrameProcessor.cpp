#include "pch.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Capture::Frames;

const int  VideoCameraFrameProcessor::kImageWidth = 640;
const wchar_t  VideoCameraFrameProcessor::kSensorName[3] = L"PV";

IAsyncAction VideoCameraFrameProcessor::InitializeAsync(
    std::shared_ptr<IVideoFrameSink> pFrameSink,
    long long minDelta)
{
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugStringW(L"VideoCameraFrameProcessor::InitializeAsync: Creating processor for Video Camera. \n");
#endif
    m_pFrameSink = pFrameSink;

    m_minDelta = minDelta;

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
    m_mediaFrameReader = co_await mediaCapture.CreateFrameReaderAsync(selectedSource);

#if DBG_ENABLE_INFO_LOGGING
    OutputDebugStringW(L"VideoCameraFrameProcessor::InitializeAsync: Done. \n");
#endif
}

IAsyncAction VideoCameraFrameProcessor::StartAsync()
{
    m_fExit = false;

#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"VideoCameraFrameProcessor::StartAsync: Starting video frame acquisition...\n");
#endif

    MediaFrameReaderStartStatus status = co_await m_mediaFrameReader.StartAsync();
    winrt::check_bool(status == MediaFrameReaderStartStatus::Success);

    m_processThread = std::thread(FrameProcesingThread, this);

    m_OnFrameArrivedRegistration = m_mediaFrameReader.FrameArrived(
        { this, &VideoCameraFrameProcessor::OnFrameArrived });

    isRunning = true;

#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"VideoCameraFrameProcessor::StartAsync: Done.\n");
#endif
}

void VideoCameraFrameProcessor::Stop()
{
    m_fExit = true;

    if (m_processThread.joinable())
    {
        m_processThread.join();
    }

    // revoke registered delegate
    m_mediaFrameReader.FrameArrived(m_OnFrameArrivedRegistration);

    m_latestFrame = nullptr;

    isRunning = false;
}

void VideoCameraFrameProcessor::OnFrameArrived(
    const MediaFrameReader& sender,
    const MediaFrameArrivedEventArgs& args)
{
    if (MediaFrameReference frame = sender.TryAcquireLatestFrame())
    {
        std::lock_guard<std::shared_mutex> lock(m_frameMutex);
        m_latestFrame = frame;
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"VideoCameraFrameProcessor::OnFrameArrived: Updated frame.\n");
#endif
    }
}

void VideoCameraFrameProcessor::FrameProcesingThread(VideoCameraFrameProcessor* pProcessor)
{
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugString(L"VideoCameraFrameProcessor::FrameProcesingThread: Starting processing thread.\n");
#endif
    while (!pProcessor->m_fExit)
    {
        std::lock_guard<std::shared_mutex> reader_guard(pProcessor->m_frameMutex);
        if (pProcessor->m_latestFrame)
        {
            MediaFrameReference frame = pProcessor->m_latestFrame;
            long long timestamp = pProcessor->m_converter.RelativeTicksToAbsoluteTicks(
                HundredsOfNanoseconds(frame.SystemRelativeTime().Value().count())).count();
            if (timestamp != pProcessor->m_latestTimestamp)
            {
                long long delta = timestamp - pProcessor->m_latestTimestamp;
                if (delta > pProcessor->m_minDelta)
                {
                    pProcessor->m_latestTimestamp = timestamp;
                    pProcessor->m_pFrameSink->Send(frame, timestamp);
                }
            }
        }
    }
}
