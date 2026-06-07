using System.Collections.ObjectModel;
using System.Globalization;
using System.Windows;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using InOutVoiceBridge.App.Services;

namespace InOutVoiceBridge.App.ViewModels;

public partial class MainViewModel : ObservableObject, IDisposable
{
    private readonly ProcessAudioSessionService _processService = new();
    private readonly AudioDeviceService _deviceService = new();
    private readonly BridgeController _bridge = new();
    private readonly DispatcherTimer _refreshTimer;

    public MainViewModel()
    {
        _bridge.LevelsUpdated += OnLevelsUpdated;
        _bridge.MicLevelUpdated += OnMicLevelUpdated;
        _bridge.StateChanged += OnStateChanged;
        _bridge.ErrorOccurred += OnErrorOccurred;

        _refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(3) };
        _refreshTimer.Tick += (_, _) => RefreshProcesses();
        _refreshTimer.Start();

        RefreshProcesses();
        RefreshDevices();
        RefreshMicDevices();
    }

    [ObservableProperty]
    private ObservableCollection<ProcessAudioInfo> _processes = [];

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(StartBridgeCommand))]
    private ProcessAudioInfo? _selectedProcess;

    [ObservableProperty]
    private ObservableCollection<AudioDeviceInfo> _outputDevices = [];

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(StartBridgeCommand))]
    private AudioDeviceInfo? _selectedOutputDevice;

    [ObservableProperty]
    private string _statusText = "Stopped";

    [ObservableProperty]
    private bool _isRunning;

    [ObservableProperty]
    private double _captureLevel;

    [ObservableProperty]
    private double _renderLevel;

    [ObservableProperty]
    private string _captureLevelDb = "-inf dB";

    [ObservableProperty]
    private string _renderLevelDb = "-inf dB";

    [ObservableProperty]
    private string _errorMessage = "";

    [ObservableProperty]
    private bool _hasError;

    [ObservableProperty]
    private bool _showVirtualCableWarning;

    [ObservableProperty]
    private string _gainDbText = "0";

    [ObservableProperty]
    private string _gainValidationMessage = "";

    [ObservableProperty]
    private bool _hasGainValidationError;

    private double _lastValidGainDb = 0.0;

    [ObservableProperty]
    private ObservableCollection<AudioDeviceInfo> _micDevices = [];

    [ObservableProperty]
    private AudioDeviceInfo? _selectedMicDevice;

    [ObservableProperty]
    private bool _isMicEnabled;

    [ObservableProperty]
    private double _micLevel;

    [ObservableProperty]
    private string _micLevelDb = "-inf dB";

    [ObservableProperty]
    private string _micGainDbText = "0";

    [ObservableProperty]
    private string _micGainValidationMessage = "";

    [ObservableProperty]
    private bool _hasMicGainValidationError;

    private double _lastValidMicGainDb = 0.0;

    partial void OnGainDbTextChanged(string value)
    {
        if (TryParseGainDb(value, out double gainDb))
        {
            _lastValidGainDb = gainDb;
            HasGainValidationError = false;
            GainValidationMessage = "";
            _bridge.SetGainDb(gainDb);
        }
        else
        {
            HasGainValidationError = true;
            GainValidationMessage = "Enter a number from -60 to +20 dB";
        }
    }

    partial void OnMicGainDbTextChanged(string value)
    {
        if (TryParseGainDb(value, out double gainDb))
        {
            _lastValidMicGainDb = gainDb;
            HasMicGainValidationError = false;
            MicGainValidationMessage = "";
            _bridge.SetMicGainDb(gainDb);
        }
        else
        {
            HasMicGainValidationError = true;
            MicGainValidationMessage = "Enter a number from -60 to +20 dB";
        }
    }

    partial void OnIsMicEnabledChanged(bool value)
    {
        if (value)
        {
            if (SelectedMicDevice == null || !IsRunning)
            {
                IsMicEnabled = false;
                return;
            }
            bool success = _bridge.StartMic(SelectedMicDevice.Id);
            if (!success)
            {
                IsMicEnabled = false;
            }
            else
            {
                _bridge.SetMicGainDb(_lastValidMicGainDb);
            }
        }
        else
        {
            _bridge.StopMic();
        }
    }

    partial void OnSelectedMicDeviceChanged(AudioDeviceInfo? value)
    {
        if (IsMicEnabled && value != null && IsRunning)
        {
            _bridge.StopMic();
            bool success = _bridge.StartMic(value.Id);
            if (!success)
            {
                IsMicEnabled = false;
            }
            else
            {
                _bridge.SetMicGainDb(_lastValidMicGainDb);
            }
        }
    }

    [RelayCommand]
    private void RefreshProcesses()
    {
        var current = SelectedProcess;
        var procs = _processService.GetAudioProcesses();
        Processes = new ObservableCollection<ProcessAudioInfo>(procs);

        if (current != null)
        {
            SelectedProcess = Processes.FirstOrDefault(p => p.ProcessId == current.ProcessId);
        }
    }

    [RelayCommand]
    private void RefreshDevices()
    {
        var current = SelectedOutputDevice;
        var devices = _deviceService.GetRenderDevices();
        OutputDevices = new ObservableCollection<AudioDeviceInfo>(devices);
        ShowVirtualCableWarning = !devices.Any(d => d.IsLikelyVirtualCable);

        if (current != null)
        {
            SelectedOutputDevice = OutputDevices.FirstOrDefault(d => d.Id == current.Id);
        }

        if (SelectedOutputDevice == null)
        {
            SelectedOutputDevice = OutputDevices.FirstOrDefault(d => d.IsLikelyVirtualCable);
        }
    }

    [RelayCommand]
    private void RefreshMicDevices()
    {
        var current = SelectedMicDevice;
        var devices = _deviceService.GetCaptureDevices();
        MicDevices = new ObservableCollection<AudioDeviceInfo>(devices);

        if (current != null)
        {
            SelectedMicDevice = MicDevices.FirstOrDefault(d => d.Id == current.Id);
        }

        if (SelectedMicDevice == null)
        {
            SelectedMicDevice = MicDevices.FirstOrDefault();
        }
    }

    private bool CanStartBridge() => SelectedProcess != null && SelectedOutputDevice != null && !IsRunning;

    [RelayCommand(CanExecute = nameof(CanStartBridge))]
    private async Task StartBridgeAsync()
    {
        if (SelectedProcess == null || SelectedOutputDevice == null) return;

        HasError = false;
        ErrorMessage = "";

        bool success = await _bridge.StartAsync(SelectedProcess.ProcessId, SelectedOutputDevice.Id);
        if (success)
        {
            _bridge.SetGainDb(_lastValidGainDb);
            StatusText = $"Bridging: {SelectedProcess.ProcessName} -> {SelectedOutputDevice.FriendlyName}";
        }
    }

    [RelayCommand]
    private void StopBridge()
    {
        _bridge.Stop();
        StatusText = "Stopped";
    }

    private void OnLevelsUpdated(float captureRms, float renderRms)
    {
        CaptureLevel = Math.Clamp(captureRms * 5.0, 0, 1);
        RenderLevel = Math.Clamp(renderRms * 5.0, 0, 1);

        CaptureLevelDb = RmsToDbString(captureRms);
        RenderLevelDb = RmsToDbString(renderRms);
    }

    private void OnMicLevelUpdated(float micRms)
    {
        MicLevel = Math.Clamp(micRms * 5.0, 0, 1);
        MicLevelDb = RmsToDbString(micRms);
    }

    private void OnStateChanged(BridgeState state)
    {
        IsRunning = state == BridgeState.Running;
        StartBridgeCommand.NotifyCanExecuteChanged();

        if (state == BridgeState.Stopped)
        {
            CaptureLevel = 0;
            RenderLevel = 0;
            CaptureLevelDb = "-inf dB";
            RenderLevelDb = "-inf dB";
            MicLevel = 0;
            MicLevelDb = "-inf dB";
            IsMicEnabled = false;
        }
    }

    private void OnErrorOccurred(string message)
    {
        HasError = true;
        ErrorMessage = message;
    }

    private static bool TryParseGainDb(string text, out double gainDb)
    {
        gainDb = 0.0;
        if (string.IsNullOrWhiteSpace(text)) return false;

        if (!double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out gainDb))
            return false;

        if (double.IsNaN(gainDb) || double.IsInfinity(gainDb))
            return false;

        return gainDb >= -60.0 && gainDb <= 20.0;
    }

    private static string RmsToDbString(float rms)
    {
        if (rms <= 0.0001f) return "-inf dB";
        double db = 20.0 * Math.Log10(rms);
        return $"{db:F1} dB";
    }

    public void Dispose()
    {
        _refreshTimer.Stop();
        _bridge.Dispose();
        GC.SuppressFinalize(this);
    }
}
