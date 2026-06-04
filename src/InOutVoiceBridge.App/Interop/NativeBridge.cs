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
    public static extern int Bridge_GetState(out int state);
}
