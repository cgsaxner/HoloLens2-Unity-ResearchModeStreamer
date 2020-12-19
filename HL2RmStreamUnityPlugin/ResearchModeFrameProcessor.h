#pragma once
class ResearchModeFrameProcessor
{
public:
	ResearchModeFrameProcessor(
		IResearchModeSensor* pLLSensor,
		HANDLE camConsentGiven,
		ResearchModeSensorConsent* camAccessConsent,
		const long long minDelta,
		std::shared_ptr<IResearchModeFrameSink> frameSink);

	~ResearchModeFrameProcessor();

protected:
	// Thread for retrieving frames
	static void CameraUpdateThread(
		ResearchModeFrameProcessor* pVLCprocessor,
		HANDLE camConsentGiven,
		ResearchModeSensorConsent* camAccessConsent);

	static void FrameProcesingThread(
		ResearchModeFrameProcessor* pProcessor);

	bool IsValidTimestamp(IResearchModeSensorFrame* pSensorFrame);

	// Mutex to access sensor frame
	std::mutex m_sensorFrameMutex;
	IResearchModeSensor* m_pRMSensor = nullptr;
	IResearchModeSensorFrame* m_pSensorFrame = nullptr;
	std::shared_ptr<IResearchModeFrameSink> m_pFrameSink = nullptr;

	bool m_fExit = false;
	// thread for reading frames
	std::thread* m_pCameraUpdateThread;
	// thread for processing frames
	std::thread* m_pProcessThread;

	UINT64 m_prevTimestamp = 0;
	// minDelta allows to enforce a certain time delay between frames
	// should be set in hundreds of nanoseconds (ms * 1e-4)
	long long m_minDelta = 0;
};

