#pragma once

#ifdef FUNCTIONS_EXPORTS  
#define FUNCTIONS_EXPORTS_API extern "C" __declspec(dllexport)   
#else  
#define FUNCTIONS_EXPORTS_API extern "C" __declspec(dllimport)   
#endif  

namespace HL2Stream
{
	FUNCTIONS_EXPORTS_API void __stdcall Initialize();

	FUNCTIONS_EXPORTS_API void StreamingToggle();

	void StartStreaming();
	
	void StopStreaming();

	void InitializeResearchModeSensors();

	winrt::Windows::Foundation::IAsyncAction
		InitializeVideoFrameProcessorAsync();

	void DisableSensors();

	void InitializeResearchModeProcessing();

	void GetRigNodeId(GUID& outGuid);

	static void CamAccessOnComplete(ResearchModeSensorConsent consent);
	static void ImuAccessOnComplete(ResearchModeSensorConsent consent);

	bool isStreaming = false;

	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem
		m_worldOrigin{ nullptr };

	IResearchModeSensorDevice* m_pSensorDevice;
	IResearchModeSensorDeviceConsent* m_pSensorDeviceConsent;
	std::vector<ResearchModeSensorDescriptor> m_sensorDescriptors;

	// video camera processing & streaming
	std::unique_ptr<VideoCameraFrameProcessor> m_pVideoFrameProcessor = nullptr;
	std::shared_ptr<VideoCameraStreamer> m_pVideoFrameStreamer = nullptr;
	winrt::Windows::Foundation::IAsyncAction m_videoFrameProcessorOperation = nullptr;

	// rm sensors processing & streaming
	IResearchModeSensor* m_pAHATSensor = nullptr;
	IResearchModeSensor* m_pLFCameraSensor = nullptr;
	IResearchModeSensor* m_pRFCameraSensor = nullptr;

	std::shared_ptr<ResearchModeFrameProcessor> m_pAHATProcessor;
	std::shared_ptr<ResearchModeFrameProcessor> m_pLFProcessor;
	std::shared_ptr<ResearchModeFrameProcessor> m_pRFProcessor;

	std::shared_ptr<ResearchModeFrameStreamer> m_pAHATStreamer = nullptr;
}