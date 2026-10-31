<#
.SYNOPSIS
  Point-and-click downloader for the Survival Wiki offline maps (Windows).

.DESCRIPTION
  Search for a place (or type a latitude and longitude), pick a radius and zoom range, check the tile count
  and size, choose your SD card, and press Download. Tiles are saved as maps\{zoom}\{x}\{y}.jpg plus
  maps\center.txt, exactly like scripts\get-map-tiles.ps1 does.

  Tiles come from the USGS National Map (US public domain, United States only). The tool pauses between
  requests and skips tiles you already have. The place search uses OpenStreetMap's Nominatim, one request
  per click on Search, nothing else is sent to it.

  Start it by double-clicking scripts\map-downloader.bat.
#>
param([switch]$SelfTest)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$inv = [cultureinfo]::InvariantCulture
$agent = "SurvivalWiki-map-tiles/1.0 (personal offline use)"
$MaxTilesWarn = 6000

function Get-Box([double]$lat, [double]$lon, [double]$km) {
  $dLat = $km / 110.574
  $dLon = $km / (111.320 * [math]::Cos($lat * [math]::PI / 180.0))
  [pscustomobject]@{ North = [math]::Min(85.0, $lat + $dLat); South = [math]::Max(-85.0, $lat - $dLat); East = $lon + $dLon; West = $lon - $dLon }
}
function Get-TileX([double]$lon, [int]$z) { [int][math]::Floor(($lon + 180.0) / 360.0 * [math]::Pow(2, $z)) }
function Get-TileY([double]$lat, [int]$z) {
  $r = $lat * [math]::PI / 180.0
  [int][math]::Floor((1.0 - [math]::Log([math]::Tan($r) + 1.0 / [math]::Cos($r)) / [math]::PI) / 2.0 * [math]::Pow(2, $z))
}
function Get-TileCount($box, [int]$minZ, [int]$maxZ) {
  $n = 0L
  for ($z = $minZ; $z -le $maxZ; $z++) {
    $w = (Get-TileX $box.East $z) - (Get-TileX $box.West $z) + 1
    $h = (Get-TileY $box.South $z) - (Get-TileY $box.North $z) + 1
    $n += [long]$w * [long]$h
  }
  $n
}

# Runs on a background runspace so the window stays responsive. $s is a synchronized hashtable.
$worker = {
  param($s)
  function TileX([double]$lon, [int]$z) { [int][math]::Floor(($lon + 180.0) / 360.0 * [math]::Pow(2, $z)) }
  function TileY([double]$lat, [int]$z) {
    $r = $lat * [math]::PI / 180.0
    [int][math]::Floor((1.0 - [math]::Log([math]::Tan($r) + 1.0 / [math]::Cos($r)) / [math]::PI) / 2.0 * [math]::Pow(2, $z))
  }
  try {
    $wc = New-Object System.Net.WebClient
    $wc.Headers.Add("User-Agent", $s.Agent)
    :zoom for ($z = $s.MinZ; $z -le $s.MaxZ; $z++) {
      $x0 = TileX $s.West $z; $x1 = TileX $s.East $z
      $y0 = TileY $s.North $z; $y1 = TileY $s.South $z
      for ($x = $x0; $x -le $x1; $x++) {
        for ($y = $y0; $y -le $y1; $y++) {
          if ($s.Cancel) { break zoom }
          $dir = Join-Path $s.Out "$z\$x"
          $file = Join-Path $dir "$y.jpg"
          if ((Test-Path $file) -and ((Get-Item $file).Length -gt 500)) { $s.Skipped++; continue }
          [void][IO.Directory]::CreateDirectory($dir)
          $url = "https://basemap.nationalmap.gov/arcgis/rest/services/USGSTopo/MapServer/tile/$z/$y/$x"
          $ok = $false
          for ($try = 1; $try -le 3 -and -not $ok; $try++) {
            try { $wc.DownloadFile($url, $file); $ok = ((Get-Item $file).Length -gt 500) }
            catch { Start-Sleep -Milliseconds (500 * $try) }
          }
          if ($ok) { $s.Done++ } else { $s.Failed++; if (Test-Path $file) { Remove-Item $file -Force } }
          Start-Sleep -Milliseconds 100
        }
      }
    }
    if (($s.Done + $s.Skipped) -gt 0) {
      $cLat = ($s.North + $s.South) / 2; $cLon = ($s.East + $s.West) / 2
      $startZ = [math]::Min($s.MaxZ, [math]::Max($s.MinZ, 13))
      $inv = [cultureinfo]::InvariantCulture
      $line = "{0} {1} {2} {3} {4}" -f $cLat.ToString($inv), $cLon.ToString($inv), $startZ, $s.MinZ, $s.MaxZ
      [IO.File]::WriteAllText((Join-Path $s.Out "center.txt"), $line + "`n", (New-Object Text.ASCIIEncoding))
    }
  } catch { $s.Error = $_.Exception.Message }
  $s.Finished = $true
}

function Start-Download($state) {
  $ps = [powershell]::Create()
  [void]$ps.AddScript($worker).AddArgument($state)
  $handle = $ps.BeginInvoke()
  [pscustomobject]@{ Ps = $ps; Handle = $handle }
}

function New-State($box, [int]$minZ, [int]$maxZ, [string]$out, [long]$total) {
  [hashtable]::Synchronized(@{
    North = $box.North; South = $box.South; East = $box.East; West = $box.West
    MinZ = $minZ; MaxZ = $maxZ; Out = $out; Agent = $agent; Total = $total
    Done = 0; Skipped = 0; Failed = 0; Cancel = $false; Finished = $false; Error = $null
  })
}

if ($SelfTest) {
  $box = Get-Box 38.9 -77.03 1.0
  $count = Get-TileCount $box 8 9
  "estimate: $count tiles at zoom 8-9"
  $tmp = Join-Path ([IO.Path]::GetTempPath()) ("maptest-" + [guid]::NewGuid().ToString("N"))
  $st = New-State $box 8 9 $tmp $count
  $job = Start-Download $st
  while (-not $st.Finished) { Start-Sleep -Milliseconds 200 }
  $job.Ps.Dispose()
  "downloaded=$($st.Done) skipped=$($st.Skipped) failed=$($st.Failed) error=$($st.Error)"
  "files: " + (Get-ChildItem $tmp -Recurse -File | Measure-Object).Count
  Get-Content (Join-Path $tmp "center.txt")
  Remove-Item $tmp -Recurse -Force
  return
}

# ---------------------------------------------------------------- window
$form = New-Object Windows.Forms.Form
$form.Text = "Survival Wiki - Offline Map Downloader"
$form.ClientSize = New-Object Drawing.Size(520, 430)
$form.FormBorderStyle = "FixedDialog"; $form.MaximizeBox = $false; $form.StartPosition = "CenterScreen"
$form.Font = New-Object Drawing.Font("Segoe UI", 9)

function Add-Ctl($ctl, $x, $y, $w, $h = 24) {
  $ctl.Location = New-Object Drawing.Point($x, $y); $ctl.Size = New-Object Drawing.Size($w, $h)
  $form.Controls.Add($ctl); $ctl
}
function Add-Label($text, $x, $y, $w = 90) {
  $l = New-Object Windows.Forms.Label; $l.Text = $text; $l.TextAlign = "MiddleLeft"
  Add-Ctl $l $x $y $w | Out-Null; $l
}
function New-Num($min, $max, $val, $dec = 0) {
  $n = New-Object Windows.Forms.NumericUpDown
  $n.Minimum = $min; $n.Maximum = $max; $n.DecimalPlaces = $dec; $n.Value = $val; $n
}

Add-Label "Find a place" 15 15 | Out-Null
$txtPlace = Add-Ctl (New-Object Windows.Forms.TextBox) 110 15 290
$btnFind = Add-Ctl (New-Object Windows.Forms.Button) 410 14 95 26
$btnFind.Text = "Search"
$cboFound = Add-Ctl (New-Object Windows.Forms.ComboBox) 110 48 395
$cboFound.DropDownStyle = "DropDownList"

Add-Label "Latitude" 15 90 | Out-Null
$txtLat = Add-Ctl (New-Object Windows.Forms.TextBox) 110 90 120
Add-Label "Longitude" 250 90 70 | Out-Null
$txtLon = Add-Ctl (New-Object Windows.Forms.TextBox) 325 90 120

Add-Label "Radius (km)" 15 128 | Out-Null
$numRadius = Add-Ctl (New-Num 0.5 50 5 1) 110 128 80
Add-Label "Zoom from" 15 166 | Out-Null
$numMin = Add-Ctl (New-Num 0 18 10) 110 166 55
Add-Label "to" 172 166 22 | Out-Null
$numMax = Add-Ctl (New-Num 0 18 15) 198 166 55
$hint = Add-Label "10 is a region, 13 a town, 15-16 shows streets and trails." 15 196 490
$hint.ForeColor = [Drawing.Color]::DimGray

Add-Label "Save to" 15 232 | Out-Null
$defaultOut = Join-Path (Split-Path $PSScriptRoot -Parent) "sdcard\maps"
$txtOut = Add-Ctl (New-Object Windows.Forms.TextBox) 110 232 290
$txtOut.Text = $defaultOut
$btnBrowse = Add-Ctl (New-Object Windows.Forms.Button) 410 231 45 26
$btnBrowse.Text = "..."
$btnSd = Add-Ctl (New-Object Windows.Forms.Button) 460 231 45 26
$btnSd.Text = "SD"

$lblEst = Add-Ctl (New-Object Windows.Forms.Label) 15 272 490 24
$lblEst.Font = New-Object Drawing.Font("Segoe UI", 10, [Drawing.FontStyle]::Bold)
$bar = Add-Ctl (New-Object Windows.Forms.ProgressBar) 15 308 490 22
$lblStat = Add-Ctl (New-Object Windows.Forms.Label) 15 336 490 24
$btnGo = Add-Ctl (New-Object Windows.Forms.Button) 15 376 240 36
$btnGo.Text = "Download"; $btnGo.Font = New-Object Drawing.Font("Segoe UI", 10, [Drawing.FontStyle]::Bold)
$btnStop = Add-Ctl (New-Object Windows.Forms.Button) 265 376 240 36
$btnStop.Text = "Cancel"; $btnStop.Enabled = $false

$script:found = @()
$script:job = $null
$script:state = $null

function Read-Box {
  $lat = 0.0; $lon = 0.0
  if (-not [double]::TryParse($txtLat.Text.Trim(), "Float", $inv, [ref]$lat)) { return $null }
  if (-not [double]::TryParse($txtLon.Text.Trim(), "Float", $inv, [ref]$lon)) { return $null }
  if ([math]::Abs($lat) -gt 85 -or [math]::Abs($lon) -gt 180) { return $null }
  Get-Box $lat $lon ([double]$numRadius.Value)
}
function Update-Estimate {
  $box = Read-Box
  if (-not $box) { $lblEst.Text = "Enter a place or a latitude and longitude."; $lblEst.ForeColor = [Drawing.Color]::DimGray; $btnGo.Enabled = $false; return }
  if ($numMin.Value -gt $numMax.Value) { $lblEst.Text = "The first zoom must not be bigger than the second."; $lblEst.ForeColor = [Drawing.Color]::Firebrick; $btnGo.Enabled = $false; return }
  $n = Get-TileCount $box ([int]$numMin.Value) ([int]$numMax.Value)
  $mb = [math]::Round($n * 35 / 1024)
  $lblEst.Text = "{0:N0} tiles, roughly {1:N0} MB" -f $n, $mb
  $lblEst.ForeColor = if ($n -gt $MaxTilesWarn) { [Drawing.Color]::DarkOrange } else { [Drawing.Color]::DarkGreen }
  $btnGo.Enabled = ($null -eq $script:job)
}

$btnFind.Add_Click({
  $q = $txtPlace.Text.Trim()
  if (-not $q) { return }
  $btnFind.Enabled = $false; $lblStat.Text = "Searching..."; $form.Refresh()
  try {
    $url = "https://nominatim.openstreetmap.org/search?format=json&limit=6&countrycodes=us&q=" + [uri]::EscapeDataString($q)
    $r = Invoke-RestMethod -Uri $url -UserAgent $agent -TimeoutSec 20
    $script:found = @($r)
    $cboFound.Items.Clear()
    foreach ($p in $script:found) { [void]$cboFound.Items.Add($p.display_name) }
    if ($script:found.Count -gt 0) { $cboFound.SelectedIndex = 0; $lblStat.Text = "$($script:found.Count) match(es). Pick one from the list." }
    else { $lblStat.Text = "No US match. Try a town and state, or type latitude and longitude." }
  } catch { $lblStat.Text = "Search failed: $($_.Exception.Message)" }
  $btnFind.Enabled = $true
})
$txtPlace.Add_KeyDown({ if ($_.KeyCode -eq "Return") { $btnFind.PerformClick(); $_.SuppressKeyPress = $true } })
$cboFound.Add_SelectedIndexChanged({
  $i = $cboFound.SelectedIndex
  if ($i -ge 0 -and $i -lt $script:found.Count) {
    $txtLat.Text = ([double]::Parse($script:found[$i].lat, $inv)).ToString("0.0000", $inv)
    $txtLon.Text = ([double]::Parse($script:found[$i].lon, $inv)).ToString("0.0000", $inv)
  }
})
foreach ($c in @($txtLat, $txtLon)) { $c.Add_TextChanged({ Update-Estimate }) }
foreach ($c in @($numRadius, $numMin, $numMax)) { $c.Add_ValueChanged({ Update-Estimate }) }

$btnBrowse.Add_Click({
  $d = New-Object Windows.Forms.FolderBrowserDialog
  $d.Description = "Choose the maps folder (on the SD card it must be named maps at the root)."
  if ($d.ShowDialog() -eq "OK") { $txtOut.Text = $d.SelectedPath }
})
$btnSd.Add_Click({
  $drv = @([IO.DriveInfo]::GetDrives() | Where-Object { $_.DriveType -eq "Removable" -and $_.IsReady })
  if ($drv.Count -eq 0) { [Windows.Forms.MessageBox]::Show("No SD card found. Plug it in, or use the ... button to pick a folder.", "SD card") | Out-Null; return }
  $txtOut.Text = Join-Path $drv[0].RootDirectory.FullName "maps"
  $lblStat.Text = "Saving to " + $txtOut.Text
})

$timer = New-Object Windows.Forms.Timer; $timer.Interval = 250
$timer.Add_Tick({
  $s = $script:state; if (-not $s) { return }
  $seen = $s.Done + $s.Skipped + $s.Failed
  $bar.Value = [int][math]::Min(100, [math]::Floor(100.0 * $seen / [math]::Max(1, $s.Total)))
  $lblStat.Text = "{0:N0} of {1:N0}  ({2:N0} new, {3:N0} already there, {4:N0} failed)" -f $seen, $s.Total, $s.Done, $s.Skipped, $s.Failed
  if ($s.Finished) {
    $timer.Stop(); $script:job.Ps.Dispose(); $script:job = $null; $script:state = $null
    $btnStop.Enabled = $false; $btnGo.Enabled = $true
    foreach ($c in $form.Controls) { if ($c -ne $btnStop) { $c.Enabled = $true } }
    if ($s.Error) { $lblStat.Text = "Stopped: " + $s.Error }
    elseif ($s.Cancel) { $lblStat.Text += "  - cancelled" }
    else {
      $bar.Value = 100
      [Windows.Forms.MessageBox]::Show("Done: $($s.Done) downloaded, $($s.Skipped) already there, $($s.Failed) failed.`n`nOn the SD card the folder must be named 'maps' at the root.", "Map download finished") | Out-Null
    }
    Update-Estimate
  }
})

$btnGo.Add_Click({
  $box = Read-Box; if (-not $box) { return }
  $minZ = [int]$numMin.Value; $maxZ = [int]$numMax.Value
  $n = Get-TileCount $box $minZ $maxZ
  if ($n -gt $MaxTilesWarn) {
    $a = [Windows.Forms.MessageBox]::Show(("{0:N0} tiles is a lot (about {1:N0} MB) and takes a long time. Continue?" -f $n, [math]::Round($n * 35 / 1024)), "Big download", "YesNo", "Warning")
    if ($a -ne "Yes") { return }
  }
  $out = $txtOut.Text.Trim()
  if (-not $out) { return }
  $script:state = New-State $box $minZ $maxZ $out $n
  $script:job = Start-Download $script:state
  foreach ($c in $form.Controls) { if ($c -ne $btnStop -and $c -ne $bar -and $c -ne $lblStat -and $c -ne $lblEst) { $c.Enabled = $false } }
  $btnStop.Enabled = $true; $bar.Value = 0; $timer.Start()
})
$btnStop.Add_Click({ if ($script:state) { $script:state.Cancel = $true; $btnStop.Enabled = $false; $lblStat.Text = "Cancelling..." } })
$form.Add_FormClosing({
  if ($script:state) { $script:state.Cancel = $true; $deadline = (Get-Date).AddSeconds(10); while (-not $script:state.Finished -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 } }
})

Update-Estimate
[void]$form.ShowDialog()
