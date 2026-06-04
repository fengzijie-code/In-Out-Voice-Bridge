# VB-CABLE Setup Guide

## What is VB-CABLE?

VB-CABLE is a free virtual audio cable that creates a pair of virtual audio devices:
- **CABLE Input** (playback device) — receives audio sent to it
- **CABLE Output** (recording device) — exposes received audio as a microphone

In Out Voice Bridge sends captured audio to "CABLE Input", and you select "CABLE Output" as your microphone in Discord, OBS, Zoom, etc.

## Installation

1. Download VB-CABLE from: https://vb-audio.com/Cable/
2. Extract the ZIP file
3. **Right-click** `VBCABLE_Setup_x64.exe` and select **Run as Administrator**
4. Follow the installer prompts
5. **Restart your computer** after installation

## Verify Installation

1. Open **Windows Settings** > **System** > **Sound**
2. Under **Output**, you should see **CABLE Input (VB-Audio Virtual Cable)**
3. Under **Input**, you should see **CABLE Output (VB-Audio Virtual Cable)**

## Using with In Out Voice Bridge

1. Launch **In Out Voice Bridge**
2. Select your music app from the process list
3. Select **CABLE Input (VB-Audio Virtual Cable)** as the output device
4. Click **START**

## Selecting the Virtual Microphone in Other Apps

### Discord
1. Open **Settings** > **Voice & Video**
2. Set **Input Device** to **CABLE Output (VB-Audio Virtual Cable)**

### OBS Studio
1. Open **Settings** > **Audio**
2. Set **Mic/Auxiliary Audio** to **CABLE Output (VB-Audio Virtual Cable)**

### Zoom
1. Open **Settings** > **Audio**
2. Set **Microphone** to **CABLE Output (VB-Audio Virtual Cable)**

### Windows Sound Recorder
1. Open Sound Recorder
2. Click the microphone icon and select **CABLE Output**

## Troubleshooting

### No audio comes through
- Make sure the music app is actually playing audio
- Verify CABLE Input is selected in In Out Voice Bridge
- Check that CABLE Output is selected as microphone in your target app
- Try restarting In Out Voice Bridge

### Audio is distorted or choppy
- Close other audio-intensive applications
- Check that your system sample rate matches (usually 48000 Hz)
- Open **Sound Control Panel** > **CABLE Input** > **Properties** > **Advanced** and set to 48000 Hz

### VB-CABLE not appearing
- Reinstall VB-CABLE as Administrator
- Restart your computer
- Check Device Manager for disabled audio devices
