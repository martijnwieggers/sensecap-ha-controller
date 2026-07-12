# Testhulp: klik op client-coordinaten in het "TFT Simulator"-venster
# en/of maak een screenshot van het clientgebied.
# Gebruik: sim_drive.ps1 -Actions "click:240,338" "wait:500" "shot:C:\pad\naar.png"
param([string[]]$Actions)

Add-Type @'
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X; public int Y; }
}
'@
Add-Type -AssemblyName System.Drawing

$proc = Get-Process sensecap_sim -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $proc) { Write-Error "Simulatorvenster niet gevonden"; exit 1 }
$hwnd = [IntPtr]$proc.MainWindowHandle
[Win32]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 200

function Get-ScreenPoint([int]$cx, [int]$cy) {
    $p = New-Object Win32+POINT; $p.X = $cx; $p.Y = $cy
    [Win32]::ClientToScreen($hwnd, [ref]$p) | Out-Null
    return $p
}

foreach ($a in $Actions) {
    $parts = $a.Split(':', 2)
    switch ($parts[0]) {
        'click' {
            $xy = $parts[1].Split(',')
            $p = Get-ScreenPoint ([int]$xy[0]) ([int]$xy[1])
            [Win32]::SetCursorPos($p.X, $p.Y) | Out-Null
            Start-Sleep -Milliseconds 100
            [Win32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)  # left down
            Start-Sleep -Milliseconds 80
            [Win32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)  # left up
            Start-Sleep -Milliseconds 300
        }
        'wait' { Start-Sleep -Milliseconds ([int]$parts[1]) }
        'drag' {   # drag:x1,y1,x2,y2 — horizontale veeg in ~10 stappen
            $xy = $parts[1].Split(',')
            $p1 = Get-ScreenPoint ([int]$xy[0]) ([int]$xy[1])
            $p2 = Get-ScreenPoint ([int]$xy[2]) ([int]$xy[3])
            [Win32]::SetCursorPos($p1.X, $p1.Y) | Out-Null
            Start-Sleep -Milliseconds 100
            [Win32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
            for ($i = 1; $i -le 10; $i++) {
                $x = $p1.X + ($p2.X - $p1.X) * $i / 10
                $y = $p1.Y + ($p2.Y - $p1.Y) * $i / 10
                [Win32]::SetCursorPos([int]$x, [int]$y) | Out-Null
                Start-Sleep -Milliseconds 25
            }
            [Win32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
            Start-Sleep -Milliseconds 300
        }
        'shot' {
            $o = Get-ScreenPoint 0 0
            $bmp = New-Object System.Drawing.Bitmap(480, 480)
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            $g.CopyFromScreen($o.X, $o.Y, 0, 0, $bmp.Size)
            $g.Dispose()
            $bmp.Save($parts[1], [System.Drawing.Imaging.ImageFormat]::Png)
            $bmp.Dispose()
            Write-Output "screenshot: $($parts[1])"
        }
    }
}
