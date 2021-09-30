#pragma once

class IVideoFrameSink
{
public:
	virtual ~IVideoFrameSink() {};
	virtual void Send(
		winrt::Windows::Media::Capture::Frames::MediaFrameReference frame,
		long long pTimestamp) = 0;
};