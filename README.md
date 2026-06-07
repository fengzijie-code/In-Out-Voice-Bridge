# In Out Voice Bridge

A Windows desktop application that captures audio from a **single selected program** and routes it to a virtual microphone input — so other apps (Discord, OBS, Zoom, etc.) can receive it as mic audio. The original playback to speakers/headphones is not affected.

## How It Works

```
Music App (e.g. Spotify)  ──plays normally──>  Speakers/Headphones
       │
       │  per-process WASAPI loopback capture
       ▼
  In Out Voice Bridge
       │  volume gain (+/- dB)
       │  renders captured audio
       ▼
  VB-CABLE Input (playback endpoint)
       │
       ▼
  VB-CABLE Output (recording endpoint)  <──  selectable as "microphone"
                                              in Discord / OBS / Zoom / etc.
```

Only the selected program's audio is captured. All other system sounds are excluded.

## Features

- **Per-process audio capture** — select exactly one app; nothing else is routed
- **Live volume gain** — adjust routed audio level from -60 to +20 dB without restarting
- **Real-time level meters** — see capture (pre-gain) and render (post-gain) audio levels
- **Auto-detect virtual cable** — automatically highlights VB-CABLE in the device list
- **Self-contained installer** — single-exe setup, no .NET runtime required on the target machine

## Prerequisites

- Windows 10 21H1+ or Windows 11
- [VB-CABLE Virtual Audio Device](https://vb-audio.com/Cable/) (free)

## Installation

### From installer (recommended)

1. Download `InOutVoiceBridgeSetup.exe` from the releases
2. Run the installer — it includes everything needed
3. Launch from the Start Menu or Desktop shortcut

### Building from source

Requires:
- Visual Studio 2022 with:
  - .NET 8 SDK
  - C++ Desktop Development workload
  - Windows 10/11 SDK
- [Inno Setup 6](https://jrsoftware.org/isinfo.php) (for building the installer)

```bash
# Option A: Open in Visual Studio
src/InOutVoiceBridge.sln

# Option B: Command line
# 1. Build C++ audio engine
msbuild src/InOutVoiceBridge.AudioEngine/InOutVoiceBridge.AudioEngine.vcxproj /p:Configuration=Release /p:Platform=x64

# 2. Build .NET WPF app
dotnet build src/InOutVoiceBridge.App/InOutVoiceBridge.App.csproj -c Release

# Option C: One-click installer build
installer\build.bat
```

The `installer\build.bat` script does everything: builds the C++ DLL, publishes the .NET app as self-contained, copies the DLL, and runs Inno Setup to produce `installer\Output\InOutVoiceBridgeSetup.exe`.

## Usage

1. Install [VB-CABLE](https://vb-audio.com/Cable/) and restart your PC
2. Launch **In Out Voice Bridge**
3. Select your music app from the process list
4. Select **CABLE Input (VB-Audio Virtual Cable)** as the output device
5. Adjust the **volume gain** if needed (0 dB = default, negative = quieter, positive = louder)
6. Click **START**
7. In Discord/OBS/Zoom, set your input device to **CABLE Output**

The capture meter shows the raw input level; the render meter shows the post-gain level being sent to the virtual cable.

See [docs/VIRTUAL_CABLE_SETUP.md](docs/VIRTUAL_CABLE_SETUP.md) for detailed VB-CABLE setup instructions.

## Architecture

| Component | Technology | Purpose |
|-----------|-----------|---------|
| `InOutVoiceBridge.App` | .NET 8 / WPF | UI, process/device enumeration, bridge orchestration |
| `InOutVoiceBridge.AudioEngine` | C++ / WASAPI | Per-process audio capture, gain, & render to virtual cable |
| `InOutVoiceBridge.Tests` | xUnit | Service integration tests |

The C++ DLL uses the Windows **per-process WASAPI loopback** API (`AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK`) to capture only the selected app's audio. Captured PCM flows through a lock-free ring buffer to the render thread, which applies the user-configured dB gain and writes the result to the chosen virtual cable endpoint.

### Native API

| Export | Description |
|--------|-------------|
| `Bridge_Start(pid, deviceId)` | Start capturing from a process and rendering to a device |
| `Bridge_Stop()` | Stop the bridge |
| `Bridge_SetGainDb(gainDb)` | Set output volume gain in dB (-60 to +20, 0 = default) |
| `Bridge_GetLevels(captureRms, renderRms)` | Poll current audio levels |
| `Bridge_GetState(state)` | Poll bridge state (Stopped / Running / Error) |

## Project Structure

```
src/
  InOutVoiceBridge.sln
  InOutVoiceBridge.App/           .NET 8 WPF desktop app
    App.xaml                      Theme colors and global resources
    MainWindow.xaml               Main UI layout
    ViewModels/MainViewModel.cs   MVVM state and commands
    Services/
      BridgeController.cs         Native DLL orchestration
      ProcessAudioSessionService  Audio session enumeration
      AudioDeviceService.cs       Render endpoint enumeration
    Interop/NativeBridge.cs       P/Invoke declarations
    Assets/AppIcon.ico            App icon (all sizes)
  InOutVoiceBridge.AudioEngine/   Native C++ DLL
    dllmain.cpp                   Exported C API
    ProcessLoopbackCapture.cpp    Per-process WASAPI loopback
    WasapiRenderSink.cpp          Render to virtual cable + gain
    RingBuffer.h                  Lock-free SPSC ring buffer
    AudioFormatConverter.h        Format conversion utilities
  InOutVoiceBridge.Tests/         Unit tests
installer/
  setup.iss                       Inno Setup 6 script
  build.bat                       One-click build script
docs/
  VIRTUAL_CABLE_SETUP.md          VB-CABLE setup guide
```

## License

MIT
