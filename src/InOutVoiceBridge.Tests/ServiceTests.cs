using InOutVoiceBridge.App.Services;
using Xunit;

namespace InOutVoiceBridge.Tests;

public class AudioDeviceServiceTests
{
    [Fact]
    public void GetRenderDevices_DoesNotThrow()
    {
        var service = new AudioDeviceService();
        var devices = service.GetRenderDevices();
        Assert.NotNull(devices);
    }

    [Fact]
    public void HasVirtualCable_ReturnsBool()
    {
        var service = new AudioDeviceService();
        _ = service.HasVirtualCable();
    }
}

public class ProcessAudioSessionServiceTests
{
    [Fact]
    public void GetAudioProcesses_DoesNotThrow()
    {
        var service = new ProcessAudioSessionService();
        var processes = service.GetAudioProcesses();
        Assert.NotNull(processes);
    }
}
