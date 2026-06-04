using System.Diagnostics;
using System.Windows.Threading;
using InOutVoiceBridge.App.Interop;

namespace InOutVoiceBridge.App.Services;

public enum BridgeState
{
    Stopped = 0,
    Running = 1,
    Error = 2
}

public class BridgeController : IDisposable
{
    private DispatcherTimer? _levelTimer;
    private uint _currentPid;

    public event Action<float, float>? LevelsUpdated;
    public event Action<BridgeState>? StateChanged;
    public event Action<string>? ErrorOccurred;

    public BridgeState CurrentState { get; private set; } = BridgeState.Stopped;

    public async Task<bool> StartAsync(uint processId, string renderDeviceId)
    {
        if (CurrentState == BridgeState.Running)
            Stop();

        // Run on thread pool (MTA) — the IAudioClient from ActivateAudioInterfaceAsync
        // requires MTA for Initialize(); the WPF UI thread is STA.
        int hr = await Task.Run(() => NativeBridge.Bridge_Start(processId, renderDeviceId));
        if (hr < 0)
        {
            CurrentState = BridgeState.Error;
            StateChanged?.Invoke(CurrentState);
            ErrorOccurred?.Invoke($"Failed to start bridge (HRESULT: 0x{hr:X8}). Make sure the selected app is playing audio and a virtual cable is installed.");
            return false;
        }

        _currentPid = processId;
        CurrentState = BridgeState.Running;
        StateChanged?.Invoke(CurrentState);

        _levelTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(50) };
        _levelTimer.Tick += OnLevelTimerTick;
        _levelTimer.Start();

        return true;
    }

    public void Stop()
    {
        _levelTimer?.Stop();
        _levelTimer = null;

        NativeBridge.Bridge_Stop();
        CurrentState = BridgeState.Stopped;
        StateChanged?.Invoke(CurrentState);
        LevelsUpdated?.Invoke(0, 0);
    }

    private void OnLevelTimerTick(object? sender, EventArgs e)
    {
        if (CurrentState != BridgeState.Running) return;

        try
        {
            var process = Process.GetProcessById((int)_currentPid);
            if (process.HasExited)
            {
                Stop();
                ErrorOccurred?.Invoke("Target process has exited. Bridge stopped.");
                return;
            }
        }
        catch
        {
            Stop();
            ErrorOccurred?.Invoke("Target process has exited. Bridge stopped.");
            return;
        }

        NativeBridge.Bridge_GetLevels(out float captureRms, out float renderRms);
        LevelsUpdated?.Invoke(captureRms, renderRms);
    }

    public void Dispose()
    {
        Stop();
        GC.SuppressFinalize(this);
    }
}
