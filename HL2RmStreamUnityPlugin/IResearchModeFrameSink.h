#pragma once

class IResearchModeFrameSink
{
public:
	virtual ~IResearchModeFrameSink() {};
	virtual void Send(
		std::shared_ptr<IResearchModeSensorFrame> pSensorFrame,
		ResearchModeSensorType pSensorType) = 0;
};
