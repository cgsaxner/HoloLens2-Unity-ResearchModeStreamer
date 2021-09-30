#pragma once
class ResearchModeFrameStreamer : public IResearchModeFrameSink
{
public:
	ResearchModeFrameStreamer(
		std::wstring portName,
		const GUID& guid,
		const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& coordSystem);

	void Send(
		std::shared_ptr<IResearchModeSensorFrame> frame,
		ResearchModeSensorType pSensorType);

	//void StreamingToggle();

public:
	bool isConnected = false;

private:
	winrt::Windows::Foundation::IAsyncAction StartServer();

	void OnConnectionReceived(
		winrt::Windows::Networking::Sockets::StreamSocketListener /* sender */,
		winrt::Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs args);

	void WriteMatrix4x4(
		_In_ winrt::Windows::Foundation::Numerics::float4x4 matrix);

	void SetLocator(const GUID& guid);

	// spatial locators
	winrt::Windows::Perception::Spatial::SpatialLocator m_locator = nullptr;
	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_worldCoordSystem = nullptr;

	// socket, listener and writer
	winrt::Windows::Networking::Sockets::StreamSocketListener m_streamSocketListener;
	winrt::Windows::Networking::Sockets::StreamSocket m_streamSocket = nullptr;
	winrt::Windows::Storage::Streams::DataWriter m_writer = nullptr;
	bool m_writeInProgress = false;

	std::wstring m_portName;

	TimeConverter m_converter;
};

