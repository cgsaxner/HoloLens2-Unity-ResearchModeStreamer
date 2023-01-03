# HoloLens2-Unity-ResearchModeStreamer

Unity Plugin for accessing HoloLens2 Research Mode sensors and video camera, and streaming them to desktop. It builds upon the official [HoloLens2ForCV](https://github.com/microsoft/HoloLens2ForCV) and [HoloLensForCV](https://github.com/microsoft/HoloLensForCV) repos. 

Currently, only Depth AHAT and video camera streams are enabled, but an extension to other RM sensors should be straight forward and will probably be added in the future. The image, as well as ```rig2world``` transforms for AHAT and and ```pv2world```, ```fx``` and ```fy``` for video camera are transmitted for each frame.

## Dependencies
### Plugin
* [Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page) >= 3.3.6
* [Visual Studio 2019](https://developer.microsoft.com/en-us/windows/downloads) (community edition is fine) 
with UWP support
* [Windows SDK 18362 or higher](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk)

## Using the Plugin
1. Open the [plugin solution](https://github.com/cgsaxner/HoloLens2-Unity-ResearchModeStreamer/tree/master/HL2RmStreamUnityPlugin) in Visual Studio
2. Add path to Eigen under _Project -> Properties -> C/C++ -> General -> Additional Include Directories_ or copy the folder containing Eigen headers to ```HL2RmStreamUnityPlugin/Dependencies```.
3. Build the solution for ```Release, ARM64```.
4. In your Unity Project, create a folder ```Assets/Plugins/WSAPlayer/ARM64```.
5. Copy the  ```HL2RmStreamUnityPlugin.dll``` from ```HL2RmStreamUnityPlugin/ARM64/Release/HL2RmStreamUnityPlugin``` into the folder from step 3.
6. To call the ```Initialize``` function from the DLL, add this statement to one of your Unity scripts:
```
[DllImport("HL2RmStreamUnityPlugin", EntryPoint = "Initialize", CallingConvention = CallingConvention.StdCall)]
public static extern void InitializeDll();
``` 
6. You can call ```InitializeDll()``` from Unity. An example can be found in [UnityHL2RmStreamer](https://github.com/cgsaxner/HoloLens2-Unity-ResearchModeStreamer/tree/master/UnityHL2RmStreamer).
7. Before building the Unity Project, go to ```Build Settings -> Player Settings``` the following Capabilities are enabled: 
    *  InternetClient, InternetClientServer, PrivateNetworkClientServer, WebCam, SpatialPerception.
8. Build the Unity Project and open the solution in Visual Studio.
9. After building the Unity Project for the first time: Open the ```package.appxmanifest``` in the solution in a text editor to enable the research mode sensors:
    * add the rescap package to ```Package```:
      ```xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"```
    * add rescap to the Ignorable Namespaces:
      ```IgnorableNamespaces="... rescap"```
    * add rescap to ```Capabilities```:
      ```<rescap:Capability Name="perceptionSensorsExperimental" />```
    * save and close ```Package.appxmanifest```
10. Build solution for ```Release, ARM64``` and deploy to HoloLens2.

## Python Client
A simple client written in python for receiving and displaying the frames is available in [hololens2_simpleclient.py](https://github.com/cgsaxner/HoloLens2-Unity-ResearchModeStreamer/blob/master/py/hololens2_simpleclient.py).

