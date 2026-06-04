using System.Diagnostics;
using NAudio.CoreAudioApi;

namespace InOutVoiceBridge.App.Services;

public record ProcessAudioInfo(uint ProcessId, string ProcessName, string WindowTitle, string ExePath);

public class ProcessAudioSessionService
{
    public List<ProcessAudioInfo> GetAudioProcesses()
    {
        var result = new Dictionary<uint, ProcessAudioInfo>();

        try
        {
            using var enumerator = new MMDeviceEnumerator();
            var defaultDevice = enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
            var sessionManager = defaultDevice.AudioSessionManager;
            var sessions = sessionManager.Sessions;

            for (int i = 0; i < sessions.Count; i++)
            {
                var session = sessions[i];
                uint pid = session.GetProcessID;
                if (pid == 0) continue;

                try
                {
                    var process = Process.GetProcessById((int)pid);
                    if (!result.ContainsKey(pid))
                    {
                        string exePath = "";
                        try { exePath = process.MainModule?.FileName ?? ""; } catch { }

                        result[pid] = new ProcessAudioInfo(
                            pid,
                            process.ProcessName,
                            process.MainWindowTitle,
                            exePath);
                    }
                }
                catch { }
            }
        }
        catch { }

        return result.Values.ToList();
    }
}
