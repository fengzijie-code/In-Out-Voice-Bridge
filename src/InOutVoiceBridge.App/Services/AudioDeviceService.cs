using NAudio.CoreAudioApi;

namespace InOutVoiceBridge.App.Services;

public record AudioDeviceInfo(string Id, string FriendlyName, bool IsLikelyVirtualCable);

public class AudioDeviceService
{
    private static readonly string[] VirtualCableKeywords = ["cable", "virtual", "vb-audio", "voicemeeter"];

    public List<AudioDeviceInfo> GetRenderDevices()
    {
        var result = new List<AudioDeviceInfo>();

        try
        {
            using var enumerator = new MMDeviceEnumerator();
            var devices = enumerator.EnumerateAudioEndPoints(DataFlow.Render, DeviceState.Active);

            foreach (var device in devices)
            {
                string name = device.FriendlyName;
                bool isVirtual = VirtualCableKeywords.Any(k =>
                    name.Contains(k, StringComparison.OrdinalIgnoreCase));

                result.Add(new AudioDeviceInfo(device.ID, name, isVirtual));
            }
        }
        catch { }

        return result;
    }

    public bool HasVirtualCable()
    {
        return GetRenderDevices().Any(d => d.IsLikelyVirtualCable);
    }
}
