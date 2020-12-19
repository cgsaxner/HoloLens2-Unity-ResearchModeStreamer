#pragma once

#define NOMINMAX

#include "targetver.h"

#include <stdio.h>
#include <wchar.h>
#include <comdef.h>
#include <MemoryBuffer.h>
#include <deque>
#include <codecvt>

#include <winrt\base.h>
#include <winrt\Windows.Foundation.h>
#include <winrt\Windows.Foundation.Collections.h>
#include <winrt\Windows.Networking.Sockets.h>
#include <winrt\Windows.Storage.Streams.h>
#include <winrt\Windows.Perception.Spatial.h>
#include <winrt\Windows.Perception.Spatial.Preview.h>
#include <winrt\Windows.Media.Capture.Frames.h>
#include <winrt\Windows.Media.Devices.Core.h>
#include <winrt\Windows.Graphics.Imaging.h>

#include "TimeConverter.h"
#include "ResearchModeApi.h"
#include "IResearchModeFrameSink.h"
#include "ResearchModeFrameProcessor.h"
#include "VideoCameraStreamer.h"
#include "Streamer.h"
