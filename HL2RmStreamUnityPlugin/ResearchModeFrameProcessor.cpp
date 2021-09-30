#include "pch.h"

#define DBG_ENABLE_VERBOSE_LOGGING 0
#define DBG_ENABLE_INFO_LOGGING 1
#define DBG_ENABLE_ERROR_LOGGING 1

using namespace winrt::Windows::Perception::Spatial;

ResearchModeFrameProcessor::ResearchModeFrameProcessor(
    IResearchModeSensor* pLLSensor,
    HANDLE camConsentGiven,
    ResearchModeSensorConsent* camAccessConsent,
    const unsigned long long minDelta,
    std::shared_ptr<IResearchModeFrameSink> frameSink) :
    m_pRMSensor(pLLSensor),
    m_camConsentGiven(camConsentGiven),
    m_pCamAccessConsent(camAccessConsent),
    m_minDelta(minDelta),
    m_pFrameSink(frameSink)
{
    m_pRMSensor->AddRef();
    m_pSensorFrame = nullptr;
    m_fExit = false;

#if DBG_ENABLE_INFO_LOGGING
    wchar_t msgBuffer[200];
    swprintf_s(msgBuffer, L"ResearchModeFrameProcessor: Created processor for sensor %ls\n",
        pLLSensor->GetFriendlyName());
    OutputDebugStringW(msgBuffer);
#endif
}

ResearchModeFrameProcessor::~ResearchModeFrameProcessor()
{
    m_fExit = true;
    if (m_cameraUpdateThread.joinable())
    {
        m_cameraUpdateThread.join();
    }
    if (m_pRMSensor)
    {
        m_pRMSensor->CloseStream();
        m_pRMSensor->Release();
    }
    if (m_processThread.joinable())
    {
        m_processThread.join();
    }
}

void ResearchModeFrameProcessor::Stop()
{
    m_fExit = true;
    if (m_cameraUpdateThread.joinable())
    {
        m_cameraUpdateThread.join();
    }
    if (m_pRMSensor)
    {
        m_pRMSensor->CloseStream();
    }
    if (m_processThread.joinable())
    {
        m_processThread.join();
    }
    isRunning = false;
}

void ResearchModeFrameProcessor::Start()
{
    m_fExit = false;
    m_cameraUpdateThread = std::thread(CameraUpdateThread, this, m_camConsentGiven, m_pCamAccessConsent);
    m_processThread = std::thread(FrameProcessingThread, this);
    isRunning = true;
}


void ResearchModeFrameProcessor::CameraUpdateThread(
    ResearchModeFrameProcessor* pResearchModeFrameProcessor,
    HANDLE camConsentGiven,
    ResearchModeSensorConsent* camAccessConsent)
{
    HRESULT hr = S_OK;

    // wait for the event to be set and check for the consent provided by the user.
    DWORD waitResult = WaitForSingleObject(camConsentGiven, INFINITE);
    if (waitResult == WAIT_OBJECT_0)
    {
        switch (*camAccessConsent)
        {
        case ResearchModeSensorConsent::Allowed:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Access is granted. \n");
            break;
        case ResearchModeSensorConsent::DeniedBySystem:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Access is denied by the system. \n");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::DeniedByUser:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Access is denied by the user. \n");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::NotDeclaredByApp:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Capability is not declared in the app manifest. \n");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::UserPromptRequired:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Capability user prompt required. \n");
            hr = E_ACCESSDENIED;
            break;
        default:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Access is denied by the system. \n");
            hr = E_ACCESSDENIED;
            break;
        }
    }
    else
    {
        hr = E_UNEXPECTED;
    }

    if (SUCCEEDED(hr))
    {
        // try to open the camera stream
        hr = pResearchModeFrameProcessor->m_pRMSensor->OpenStream();
        if (FAILED(hr))
        {
            pResearchModeFrameProcessor->m_pRMSensor->Release();
            pResearchModeFrameProcessor->m_pRMSensor = nullptr;
#if DBG_ENABLE_ERROR_LOGGING
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Opening the Stream failed.\n");
#endif
        }
#if DBG_ENABLE_INFO_LOGGING
        else
        {
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Starting acquisition loop!\n");
        }
#endif
        // frame acquisition loop
        while (!pResearchModeFrameProcessor->m_fExit && pResearchModeFrameProcessor->m_pRMSensor)
        {
            hr = S_OK;
            // try to grab the next frame
            IResearchModeSensorFrame* pSensorFrame = nullptr;
            hr = pResearchModeFrameProcessor->m_pRMSensor->GetNextBuffer(&pSensorFrame);

            if (SUCCEEDED(hr))
            {
                std::lock_guard<std::mutex> guard(pResearchModeFrameProcessor->
                    m_sensorFrameMutex);

                std::shared_ptr<IResearchModeSensorFrame> spSensorFrame(pSensorFrame, [](IResearchModeSensorFrame* sf) { sf->Release(); });

                pResearchModeFrameProcessor->m_pSensorFrame = spSensorFrame;
#if DBG_ENABLE_VERBOSE_LOGGING
                OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Updated frame.\n");
#endif
            }
            else
            {
#if DBG_ENABLE_ERROR_LOGGING
                OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Failed getting frame.\n");
#endif
            }
        }

        // if thread should exit...
        if (pResearchModeFrameProcessor->m_pRMSensor)
        {
#if DBG_ENABLE_INFO_LOGGING
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Closing the stream.\n");
#endif
            pResearchModeFrameProcessor->m_pRMSensor->CloseStream();
        }
    }
}


void ResearchModeFrameProcessor::FrameProcessingThread(
    ResearchModeFrameProcessor* pProcessor)
{
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugString(L"ResearchModeFrameProcessor::CameraStreamThread: Starting processing thread.\n");
#endif
    while (!pProcessor->m_fExit && pProcessor->m_pFrameSink)
    {
        std::lock_guard<std::mutex> reader_guard(pProcessor->m_sensorFrameMutex);
        if (pProcessor->m_pSensorFrame)
        {
            if (pProcessor->IsValidTimestamp(pProcessor->m_pSensorFrame))
            {
                pProcessor->m_pFrameSink->Send(
                    pProcessor->m_pSensorFrame,
                    pProcessor->m_pRMSensor->GetSensorType());
            }
        }
    }
}

bool ResearchModeFrameProcessor::IsValidTimestamp(
    std::shared_ptr<IResearchModeSensorFrame> pSensorFrame)
{
    ResearchModeSensorTimestamp timestamp;
    if (pSensorFrame)
    {
        winrt::check_hresult(pSensorFrame->GetTimeStamp(&timestamp));
        if (m_prevTimestamp == timestamp.HostTicks)
        {
            return false;
        }
        auto delta = timestamp.HostTicks - m_prevTimestamp;
        if (delta < m_minDelta)
        {
            return false;
        }
        m_prevTimestamp = timestamp.HostTicks;
        return true;
    }
    else
    {
        return false;
    }
}

