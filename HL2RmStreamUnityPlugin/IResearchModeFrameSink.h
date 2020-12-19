#pragma once

class IResearchModeFrameSink
{
public:
	virtual ~IResearchModeFrameSink() {};
	virtual void Send(
		IResearchModeSensorFrame* pSensorFrame,
		ResearchModeSensorType pSensorType) = 0;
}; 
