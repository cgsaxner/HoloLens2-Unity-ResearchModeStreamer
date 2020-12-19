#pragma once

#ifdef FUNCTIONS_EXPORTS  
#define FUNCTIONS_EXPORTS_API extern "C" __declspec(dllexport)   
#else  
#define FUNCTIONS_EXPORTS_API extern "C" __declspec(dllimport)   
#endif  

namespace HL2Stream
{
	FUNCTIONS_EXPORTS_API void __stdcall StartStreaming();

	FUNCTIONS_EXPORTS_API void StreamingToggle();

	void InitializeResearchModeSensors();

	winrt::Windows::Foundation::IAsyncAction
		InitializeVideoFrameProcessorAsync();

	void DisableSensors();

	void InitializeResearchModeProcessing();

	void GetRigNodeId(GUID& outGuid);

	static void CamAccessOnComplete(ResearchModeSensorConsent consent);
	static void ImuAccessOnComplete(ResearchModeSensorConsent consent);

	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem
		m_worldOrigin{ nullptr };
	std::wstring m_patient;

	IResearchModeSensorDevice* m_pSensorDevice;
	IResearchModeSensorDeviceConsent* m_pSensorDeviceConsent;
	std::vector<ResearchModeSensorDescriptor> m_sensorDescriptors;

	std::unique_ptr<VideoCameraStreamer> m_videoFrameProcessor = nullptr;
	winrt::Windows::Foundation::IAsyncAction m_videoFrameProcessorOperation = nullptr;

	// sensor
	IResearchModeSensor* m_pAHATSensor = nullptr;

	// camera streamers
	std::shared_ptr<Streamer> m_pAHATStreamer;
	
	// sensor processors
	std::shared_ptr<ResearchModeFrameProcessor> m_pAHATProcessor;
}