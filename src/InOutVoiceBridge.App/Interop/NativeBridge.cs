using System.Runtime.InteropServices;

namespace InOutVoiceBridge.App.Interop;

internal static class NativeBridge
{
    private const string DllName = "InOutVoiceBridge.AudioEngine.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_Start(
        uint targetPid,
        [MarshalAs(UnmanagedType.LPWStr)] string renderDeviceId);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_Stop();

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_GetLevels(out float captureRms, out float renderRms);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_SetGainDb(float gainDb);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_GetState(out int state);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_StartMic(
        [MarshalAs(UnmanagedType.LPWStr)] string micDeviceId);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_StopMic();

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_SetMicGainDb(float gainDb);

    [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
    public static extern int Bridge_GetMicLevel(out float micRms);
}
