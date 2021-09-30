using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class StartStreamer : MonoBehaviour
{
#if ENABLE_WINMD_SUPPORT
    [DllImport("HL2RmStreamUnityPlugin", EntryPoint = "Initialize", CallingConvention = CallingConvention.StdCall)]
    public static extern void InitializeDll();
#endif

    // Start is called before the first frame update
    void Start()
    {
#if ENABLE_WINMD_SUPPORT
        InitializeDll();
#endif
    }

    // Update is called once per frame
    void Update()
    {
     
    }
}
