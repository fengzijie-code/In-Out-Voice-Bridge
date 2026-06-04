# In Out Voice Bridge

A Windows 11 desktop application that captures audio from a **single selected program** and routes it to a virtual microphone input — so other apps (Discord, OBS, Zoom, etc.) can receive it as mic audio. The original playback to speakers/headphones is not affected.

## How It Works

```
Music App (e.g. Spotify)  ──plays normally──>  Speakers/Headphones
       │
       │  per-process WASAPI loopback capture
       ▼
  In Out Voice Bridge
       │
       │  renders captured audio
       ▼
  VB-CABLE Input (playback endpoint)
       │
       ▼
  VB-CABLE Output (recording endpoint)  <──  selectable as "microphone"
                                              in Discord / OBS / Zoom / etc.
```

Only the selected program's audio is captured. All other system sounds are excluded.

## Prerequisites

- Windows 10 21H1+ or Windows 11
- [VB-CABLE Virtual Audio Device](https://vb-audio.com/Cable/) (free)
- Visual Studio 2022 with:
  - .NET 8 SDK
  - C++ Desktop Development workload
  - Windows 10/11 SDK

## Building

```bash
# Open in Visual Studio
src/InOutVoiceBridge.sln

# Or from command line (build C++ DLL first, then .NET app)
msbuild src/InOutVoiceBridge.AudioEngine/InOutVoiceBridge.AudioEngine.vcxproj /p:Configuration=Release /p:Platform=x64
dotnet build src/InOutVoiceBridge.App/InOutVoiceBridge.App.csproj -c Release
```

## Usage

1. Install [VB-CABLE](https://vb-audio.com/Cable/) and restart your PC
2. Launch **In Out Voice Bridge**
3. Select your music app from the process list
4. Select **CABLE Input (VB-Audio Virtual Cable)** as the output device
5. Click **START**
6. In Discord/OBS/Zoom, set your input device to **CABLE Output**

See [docs/VIRTUAL_CABLE_SETUP.md](docs/VIRTUAL_CABLE_SETUP.md) for detailed setup instructions.

## Architecture

| Component | Technology | Purpose |
|-----------|-----------|---------|
| `InOutVoiceBridge.App` | .NET 8 / WPF | UI, process/device enumeration, bridge orchestration |
| `InOutVoiceBridge.AudioEngine` | C++ / WASAPI | Per-process audio capture & render to virtual cable |
| `InOutVoiceBridge.Tests` | xUnit | Service integration tests |

The C++ DLL uses the Windows **per-process WASAPI loopback** API (`AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK`) to capture only the selected app's audio, then renders it to the chosen virtual cable endpoint via a lock-free ring buffer.

## License

MIT
