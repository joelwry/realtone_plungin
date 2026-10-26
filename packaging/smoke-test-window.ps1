<#
.SYNOPSIS
  Starts the standalone application and checks, through the Win32 API, that it has a genuine
  native top-level window: caption, system menu, minimise/maximise buttons, a sizing border that
  answers hit tests on every edge and corner, size limits, maximise/restore, double-click on the
  title bar, and a clean exit on close. This is a message-level check, not a substitute for trying
  the mouse by hand.

  pwsh packaging\smoke-test-window.ps1 -Exe "C:\Program Files\RealTone Studio\RealTone Studio.exe"
#>
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [int]$MinWidth = 900,
    [int]$MinHeight = 580
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W32 {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int index);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
}
"@

$GWL_STYLE = -16
$WS_CAPTION = 0x00C00000; $WS_SYSMENU = 0x00080000; $WS_THICKFRAME = 0x00040000
$WS_MINIMIZEBOX = 0x00020000; $WS_MAXIMIZEBOX = 0x00010000
$WM_NCHITTEST = 0x0084; $WM_NCLBUTTONDBLCLK = 0x00A3
$HT = @{ CAPTION = 2; LEFT = 10; RIGHT = 11; TOP = 12; TOPLEFT = 13; TOPRIGHT = 14; BOTTOM = 15; BOTTOMLEFT = 16; BOTTOMRIGHT = 17 }
$SW_MAXIMIZE = 3; $SW_MINIMIZE = 6; $SW_RESTORE = 9
$SWP_NOZORDER = 0x4; $SWP_NOACTIVATE = 0x10

$failures = @()
function Check($name, $ok, $detail = "") {
    if ($ok) { Write-Host ("  PASS  {0} {1}" -f $name, $detail) }
    else { Write-Host ("  FAIL  {0} {1}" -f $name, $detail) -ForegroundColor Red; $script:failures += $name }
}
function Rect($h) { $r = New-Object W32+RECT; [void][W32]::GetWindowRect($h, [ref]$r); return $r }
function MakeLParam($x, $y) { return [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF)) }
function HitTest($h, $x, $y) { return [int]([W32]::SendMessage($h, $WM_NCHITTEST, [IntPtr]::Zero, (MakeLParam $x $y)).ToInt64()) }
function Settle($ms = 400) { Start-Sleep -Milliseconds $ms }

Write-Host "Starting $Exe"
$proc = Start-Process -FilePath $Exe -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 80 -and $h -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 250
    $proc.Refresh()
    if ($proc.HasExited) { throw "The application exited during start-up (exit code $($proc.ExitCode))." }
    $h = $proc.MainWindowHandle
}
Check "main window appears" ($h -ne [IntPtr]::Zero)
if ($h -eq [IntPtr]::Zero) { $proc.Kill(); exit 1 }
Settle 1500

$style = [W32]::GetWindowLong($h, $GWL_STYLE)
Check "native caption"        (($style -band $WS_CAPTION) -eq $WS_CAPTION)
Check "system menu"           (($style -band $WS_SYSMENU) -ne 0)
Check "sizing border"         (($style -band $WS_THICKFRAME) -ne 0)
Check "minimise button"       (($style -band $WS_MINIMIZEBOX) -ne 0)
Check "maximise button"       (($style -band $WS_MAXIMIZEBOX) -ne 0)

$r = Rect $h
$w = $r.Right - $r.Left; $hgt = $r.Bottom - $r.Top
$screenW = [W32]::GetSystemMetrics(0); $screenH = [W32]::GetSystemMetrics(1)
Write-Host ("  window {0}x{1} at ({2},{3}) on a {4}x{5} screen, {6} dpi" -f $w, $hgt, $r.Left, $r.Top, $screenW, $screenH, [W32]::GetDpiForWindow($h))
Check "initial size fits the screen" ($w -le $screenW -and $hgt -le $screenH)
Check "initial size respects the minimum" ($w -ge $MinWidth -and $hgt -ge $MinHeight)

# Hit tests on every edge and corner, and the title bar.
$mx = [int](($r.Left + $r.Right) / 2); $my = [int](($r.Top + $r.Bottom) / 2)
$e = 2   # inside the border by a couple of pixels
$expect = @(
    @("left edge",         ($r.Left + $e),      $my,               $HT.LEFT),
    @("right edge",        ($r.Right - $e - 1), $my,               $HT.RIGHT),
    @("bottom edge",       $mx,                 ($r.Bottom - $e - 1), $HT.BOTTOM),
    @("top-left corner",   ($r.Left + $e),      ($r.Top + $e),     $HT.TOPLEFT),
    @("top-right corner",  ($r.Right - $e - 1), ($r.Top + $e),     $HT.TOPRIGHT),
    @("bottom-left corner",($r.Left + $e),      ($r.Bottom - $e - 1), $HT.BOTTOMLEFT),
    @("bottom-right corner",($r.Right - $e - 1),($r.Bottom - $e - 1), $HT.BOTTOMRIGHT)
)
foreach ($t in $expect) {
    $got = HitTest $h $t[1] $t[2]
    Check ("resize from " + $t[0]) ($got -eq $t[3]) ("(hit-test $got)")
}
# Caption: roughly 15 px below the top edge, away from the system buttons
$cap = HitTest $h ($r.Left + 120) ($r.Top + 14)
Check "title bar drags the window" ($cap -eq $HT.CAPTION) "(hit-test $cap)"

# Resizing within, below and above the limits.
[void][W32]::SetWindowPos($h, [IntPtr]::Zero, 40, 40, 1100, 700, $SWP_NOZORDER -bor $SWP_NOACTIVATE); Settle
$r = Rect $h
Check "resize to 1100x700" ((($r.Right - $r.Left) -eq 1100) -and (($r.Bottom - $r.Top) -eq 700)) ("(got {0}x{1})" -f ($r.Right - $r.Left), ($r.Bottom - $r.Top))
[void][W32]::SetWindowPos($h, [IntPtr]::Zero, 40, 40, 300, 200, $SWP_NOZORDER -bor $SWP_NOACTIVATE); Settle
$r = Rect $h
Check "cannot shrink below the usable minimum" ((($r.Right - $r.Left) -ge $MinWidth) -and (($r.Bottom - $r.Top) -ge $MinHeight)) ("(got {0}x{1})" -f ($r.Right - $r.Left), ($r.Bottom - $r.Top))
[void][W32]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, 12000, 9000, $SWP_NOZORDER -bor $SWP_NOACTIVATE); Settle
$r = Rect $h
Check "cannot grow beyond the largest screen" ((($r.Right - $r.Left) -le ($screenW + 40)) -and (($r.Bottom - $r.Top) -le ($screenH + 40))) ("(got {0}x{1})" -f ($r.Right - $r.Left), ($r.Bottom - $r.Top))
[void][W32]::SetWindowPos($h, [IntPtr]::Zero, 60, 60, 1100, 700, $SWP_NOZORDER -bor $SWP_NOACTIVATE); Settle

# Maximise, restore, minimise.
[void][W32]::ShowWindow($h, $SW_MAXIMIZE); Settle 800
Check "maximise" ([W32]::IsZoomed($h))
[void][W32]::ShowWindow($h, $SW_RESTORE); Settle 800
Check "restore from maximised" (-not [W32]::IsZoomed($h))
[void][W32]::ShowWindow($h, $SW_MINIMIZE); Settle 800
Check "minimise" ([W32]::IsIconic($h))
[void][W32]::ShowWindow($h, $SW_RESTORE); Settle 800
Check "restore from minimised" (-not [W32]::IsIconic($h))

# Double-click on the title bar maximises, and again restores.
[void][W32]::SendMessage($h, $WM_NCLBUTTONDBLCLK, [IntPtr]$HT.CAPTION, [IntPtr]::Zero); Settle 800
Check "double-click title bar maximises" ([W32]::IsZoomed($h))
[void][W32]::SendMessage($h, $WM_NCLBUTTONDBLCLK, [IntPtr]$HT.CAPTION, [IntPtr]::Zero); Settle 800
Check "double-click title bar restores" (-not [W32]::IsZoomed($h))

# The window is still responding and closes cleanly.
$proc.Refresh()
Check "application still running" (-not $proc.HasExited)
Check "window still responding" ($proc.Responding)
[void]$proc.CloseMainWindow()
$exited = $proc.WaitForExit(15000)
Check "closes cleanly from the close button" $exited
if (-not $exited) { $proc.Kill() }

if ($failures.Count -gt 0) {
    Write-Host ("`n{0} check(s) failed: {1}" -f $failures.Count, ($failures -join ", ")) -ForegroundColor Red
    exit 1
}
Write-Host "`nAll window checks passed." -ForegroundColor Green
