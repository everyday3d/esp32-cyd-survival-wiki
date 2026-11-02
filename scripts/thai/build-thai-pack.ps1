<#
.SYNOPSIS
  Builds the Thai SD card pack: pre-drawn Thai text for the buttons and titles, and Thai guide pages.

.DESCRIPTION
  The board's built-in fonts only have English letters, and Thai needs letters stacked above and below each
  other. So the Thai text is drawn here, on the PC, with the open-license Noto Sans Thai font, and the board
  shows the finished pictures.

  Inputs  (l10n\th\):
    strings.tsv            English<TAB>Thai, one per line. Used for buttons, tiles, headers, titles.
    articles\<id>.md       Thai version of a guide (same layout as sdcard\wiki\articles\<id>.md)
    fonts\                 NotoSansThai-Regular.ttf and -Bold.ttf (SIL Open Font License)
  Output  (sdcard-th\th\): atlas28.thb, atlas35.thb and p28\*.thp, p35\*.thp. Copy the "th" folder to the SD card.

  In the Thai text, put | where a line may break between words (Thai has no spaces between words).
  A normal space is kept as a space. Everything is UTF-8.
#>
param(
  [string]$Root = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
  [string]$Out = "",
  [string[]]$Boards = @("28", "35")
)
$ErrorActionPreference = "Stop"
if (-not $Out) { $Out = Join-Path $Root "sdcard-th\th" }
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
public static class ThaiPack {
  // 32bpp BGRA pixels (white text on black) -> 4-bit alpha, two pixels per byte, rows padded to a whole byte
  public static byte[] Pack(byte[] bgra, int w, int h, int stride) {
    int rs = (w + 1) / 2; byte[] o = new byte[rs * h];
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      int a = bgra[y * stride + x * 4 + 1] >> 4;
      if ((x & 1) == 0) o[y * rs + x / 2] = (byte)(a << 4); else o[y * rs + x / 2] |= (byte)a;
    }
    return o;
  }
  public static uint Fnv(byte[] d) { uint h = 2166136261; foreach (byte b in d) { h ^= b; h *= 16777619; } return h; }
}
"@

$u8 = New-Object System.Text.UTF8Encoding($false)
$l10n = Join-Path $Root "l10n\th"
$pfc = New-Object System.Drawing.Text.PrivateFontCollection
$pfc.AddFontFile((Join-Path $l10n "fonts\NotoSansThai-Regular.ttf"))
$pfc.AddFontFile((Join-Path $l10n "fonts\NotoSansThai-Bold.ttf"))
$pfc.AddFontFile((Join-Path $l10n "fonts\NotoSans-Regular.ttf"))
$pfc.AddFontFile((Join-Path $l10n "fonts\NotoSans-Bold.ttf"))
$famTh = $pfc.Families | Where-Object { $_.Name -eq "Noto Sans Thai" } | Select-Object -First 1
$famLa = $pfc.Families | Where-Object { $_.Name -eq "Noto Sans" } | Select-Object -First 1
if (-not $famTh -or -not $famLa) { throw "fonts not found: Noto Sans Thai and Noto Sans are both needed" }

# ---- board settings -------------------------------------------------------------------------------------
$BD = @{
  "28" = @{ W = 240; TX = 8;  K = 1.0 }
  "35" = @{ W = 320; TX = 10; K = 1.2 }
}
# atlas styles, matched to the board's fonts: 0 small, 1 small bold, 2 mid bold, 3 large bold, 4 extra large bold
$STY = @(@(15, $false), @(15, $true), @(20, $true), @(26, $true), @(34, $true))

$script:fontCache = @{}
function Get-Font([double]$px, [bool]$bold, [bool]$thai) {
  $k = "$px/$bold/$thai"
  if (-not $script:fontCache.ContainsKey($k)) {
    $st = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
    $f = if ($thai) { $famTh } else { $famLa }
    $script:fontCache[$k] = New-Object System.Drawing.Font($f, [single]$px, $st, [System.Drawing.GraphicsUnit]::Pixel)
  }
  $script:fontCache[$k]
}
# The Thai font has no digits or English letters, so Thai runs use it and everything else uses Noto Sans.
function Split-Runs([string]$t) {
  $r = New-Object System.Collections.Generic.List[object]
  foreach ($m in [regex]::Matches($t, "[\u0E00-\u0E7F]+|[^\u0E00-\u0E7F]+")) { $r.Add(@{ t = $m.Value; thai = ($m.Value[0] -ge [char]0x0E00 -and $m.Value[0] -le [char]0x0E7F) }) }
  , $r
}
$script:gm = [System.Drawing.Graphics]::FromImage((New-Object System.Drawing.Bitmap(1, 1)))
$script:gm.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$script:sf = [System.Drawing.StringFormat]::GenericTypographic
$script:sf.FormatFlags = $script:sf.FormatFlags -bor [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces

function Measure-W([string]$t, [double]$px, [bool]$bold) {
  if ($t.Length -eq 0) { return 0.0 }
  [double]$sum = 0
  foreach ($run in (Split-Runs $t)) { $sum += [double]$script:gm.MeasureString($run.t, (Get-Font $px $bold $run.thai), 100000, $script:sf).Width }
  $sum
}

# Draws one line of text. Returns @{ w; h; data } (4-bit alpha). Height is tall enough for Thai marks above and below.
function Render-Line([string]$t, [double]$px, [bool]$bold, [int]$minH = 0) {
  $w = [int][math]::Ceiling((Measure-W $t $px $bold)) + 3
  $h = [int][math]::Ceiling($px * 1.5) + 2
  if ($minH -gt $h) { $h = $minH }
  $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.Clear([System.Drawing.Color]::Black)
  $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
  [double]$cx = 1
  foreach ($run in (Split-Runs $t)) {
    $f = Get-Font $px $bold $run.thai
    $g.DrawString($run.t, $f, [System.Drawing.Brushes]::White, [single]$cx, [single]([math]::Round($px * 0.18)), $script:sf)
    $cx += [double]$script:gm.MeasureString($run.t, $f, 100000, $script:sf).Width
  }
  $g.Dispose()
  $bd = $bmp.LockBits((New-Object System.Drawing.Rectangle(0, 0, $w, $h)), [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $raw = New-Object byte[] ($bd.Stride * $h)
  [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $raw, 0, $raw.Length)
  $stride = $bd.Stride
  $bmp.UnlockBits($bd); $bmp.Dispose()
  @{ w = $w; h = $h; data = [ThaiPack]::Pack($raw, $w, $h, $stride) }
}

function Strip-Marks([string]$s) { ($s -replace '\|', '') }

# ---- atlas: strings.tsv -> atlasNN.thb -------------------------------------------------------------------
function Build-Atlas([string]$board) {
  $K = $BD[$board].K
  $file = Join-Path $l10n "strings.tsv"
  if (-not (Test-Path $file)) { Write-Host "no strings.tsv"; return }
  $entries = New-Object System.Collections.Generic.List[object]
  $count = 0
  foreach ($line in [IO.File]::ReadAllLines($file, $u8)) {
    if ($line.Length -eq 0 -or $line[0] -eq '#') { continue }
    $p = $line.Split("`t")
    if ($p.Count -lt 2 -or $p[1].Trim().Length -eq 0) { continue }
    $en = $p[0]; $th = Strip-Marks $p[1].Trim()
    $hash = [ThaiPack]::Fnv($u8.GetBytes($en))
    for ($s = 0; $s -lt $STY.Count; $s++) {
      $px = [math]::Round($STY[$s][0] * $K)
      $r = Render-Line $th $px $STY[$s][1]
      $key = [uint32](([uint64]$hash + [uint64]$s * 2654435761) % 4294967296)
      $entries.Add(@{ key = $key; w = $r.w; h = $r.h; data = $r.data })
    }
    $count++
  }
  $sorted = $entries | Sort-Object { $_.key }
  $ms = New-Object System.IO.MemoryStream
  $bw = New-Object System.IO.BinaryWriter($ms)
  $bw.Write([byte[]][char[]]"THA1"); $bw.Write([uint32]$sorted.Count)
  $off = 8 + 12 * $sorted.Count
  foreach ($e in $sorted) { $bw.Write([uint32]$e.key); $bw.Write([uint32]$off); $bw.Write([uint16]$e.w); $bw.Write([uint16]$e.h); $off += $e.data.Length }
  foreach ($e in $sorted) { $bw.Write([byte[]]$e.data) }
  $bw.Flush()
  New-Item -ItemType Directory -Force $Out | Out-Null
  [IO.File]::WriteAllBytes((Join-Path $Out "atlas$board.thb"), $ms.ToArray())
  Write-Host ("atlas{0}.thb: {1} strings, {2} bitmaps, {3:N0} KB" -f $board, $count, $sorted.Count, ($ms.Length / 1KB))
}

# ---- pages: articles\<id>.md -> pNN\<id>.thp -------------------------------------------------------------
# Item types: 0 text, 1 warning box rectangle, 2 rule, 3 dot, 4 picture (path stored after the item table)
function Wrap-Lines([string]$text, [double]$px, [bool]$bold, [double]$maxW) {
  $text = ($text -replace '\*\*', '' -replace '`', '').Trim()
  $toks = New-Object System.Collections.Generic.List[object]
  $parts = [regex]::Split($text, '( |\|)')
  $pendingSpace = $false
  foreach ($pt in $parts) {
    if ($pt -eq ' ') { $pendingSpace = $true; continue }
    if ($pt -eq '|') { continue }
    if ($pt.Length -eq 0) { continue }
    $toks.Add(@{ t = $pt; sp = $pendingSpace }); $pendingSpace = $false
  }
  $lines = New-Object System.Collections.Generic.List[string]
  $cur = ""
  foreach ($tk in $toks) {
    $cand = if ($cur.Length -eq 0) { $tk.t } else { $cur + $(if ($tk.sp) { " " } else { "" }) + $tk.t }
    if ((Measure-W $cand $px $bold) -le $maxW) { $cur = $cand; continue }
    if ($cur.Length -gt 0) { $lines.Add($cur); $cur = "" }
    if ((Measure-W $tk.t $px $bold) -le $maxW) { $cur = $tk.t; continue }
    # a single word too long for the line: break it between letters
    $en = [System.Globalization.StringInfo]::GetTextElementEnumerator($tk.t)
    $piece = ""
    while ($en.MoveNext()) {
      $c = [string]$en.Current
      if ((Measure-W ($piece + $c) $px $bold) -gt $maxW -and $piece.Length -gt 0) { $lines.Add($piece); $piece = "" }
      $piece += $c
    }
    $cur = $piece
  }
  if ($cur.Length -gt 0) { $lines.Add($cur) }
  , $lines
}

function Build-Page([string]$id, [string]$src, [string]$board) {
  $W = $BD[$board].W; $TX = $BD[$board].TX; $K = $BD[$board].K
  $small = ($board -eq "28")
  $TW = $W - 2 * $TX
  $items = New-Object System.Collections.Generic.List[object]
  $script:y = 8
  function Add-Item($type, $fg, $bg, $x, $y, $w, $h, $data) { $items.Add(@{ type = $type; fg = $fg; bg = $bg; x = $x; y = $y; w = $w; h = $h; data = $data }) }
  # style: name -> px, bold, line height
  $ST = @{
    h1   = @([math]::Round(21 * $K), $true,  [math]::Round(32 * $K))
    h2   = @([math]::Round(18 * $K), $true,  [math]::Round(28 * $K))
    body = @([math]::Round(15 * $K), $false, [math]::Round(24 * $K))
    cap  = @([math]::Round(13 * $K), $false, [math]::Round(20 * $K))
  }
  function Add-Text([string]$text, [int]$x0, [int]$width, [string]$kind, [int]$fg, [int]$bg, [bool]$warn) {
    $px = $ST[$kind][0]; $bold = $ST[$kind][1]; $lh = $ST[$kind][2]
    foreach ($ln in (Wrap-Lines $text $px $bold $width)) {
      if ($warn) { Add-Item 1 0 1 0 $script:y $W $lh $null }
      $r = Render-Line $ln $px $bold $lh
      Add-Item 0 $fg $bg $x0 $script:y $r.w $r.h $r.data
      $script:y += $lh
    }
  }
  $lines = [IO.File]::ReadAllText($src, $u8).Replace("`r`n", "`n").Split("`n")
  $i = 0
  if ($lines.Count -gt 1 -and $lines[0] -eq "---") {
    $i = 1
    while ($i -lt $lines.Count -and $lines[$i] -ne "---") {
      if ($lines[$i].StartsWith("title:")) { Add-Text ($lines[$i].Substring(6).Trim()) $TX $TW "h1" 0 0 $false; $script:y += 4 }
      $i++
    }
    $i++
  }
  $lastBlank = $true; $inWarn = $false
  for (; $i -lt $lines.Count; $i++) {
    $ln = $lines[$i].Trim()
    $isWarn = $ln.StartsWith(">")
    if ($inWarn -and -not $isWarn) { Add-Item 1 0 1 0 $script:y $W 8 $null; $script:y += 14; $inWarn = $false }
    if ($ln.Length -eq 0) { if (-not $lastBlank) { $script:y += 8 }; $lastBlank = $true; continue }
    $lastBlank = $false
    if ($isWarn) {
      if (-not $inWarn) { $script:y += 2; Add-Item 1 0 1 0 $script:y $W 8 $null; $script:y += 8; $inWarn = $true }
      Add-Text ($ln.Substring(1).Trim()) $(if ($small) { 18 } else { 22 }) ($W - $(if ($small) { 30 } else { 40 })) "body" 0 1 $true
    } elseif ($ln -match '^(#+)\s*(.+)$') {
      $h1 = ($Matches[1].Length -eq 1)
      $script:y += $(if ($h1) { if ($small) { 6 } else { 8 } } else { if ($small) { 4 } else { 6 } })
      Add-Text $Matches[2] $TX $TW $(if ($h1) { "h1" } else { "h2" }) $(if ($h1) { 0 } else { 1 }) 0 $false
    } elseif ($ln.StartsWith("---")) {
      Add-Item 2 0 0 0 $script:y $W $(if ($small) { 12 } else { 16 }) $null; $script:y += $(if ($small) { 12 } else { 16 })
    } elseif ($ln -match '^[-*]\s+(.+)$') {
      Add-Item 3 1 0 $(if ($small) { 10 } else { 14 }) $script:y 0 $ST.body[2] $null
      Add-Text $Matches[1] $(if ($small) { 22 } else { 28 }) ($W - $(if ($small) { 30 } else { 38 })) "body" 0 0 $false
    } elseif ($ln -match '^(\d+)\.\s+(.+)$') {
      $num = $Matches[1] + "."; $rest = $Matches[2]
      $r = Render-Line $num $ST.body[0] $true $ST.body[2]
      Add-Item 0 1 0 $(if ($small) { 8 } else { 10 }) $script:y $r.w $r.h $r.data
      Add-Text $rest $(if ($small) { 26 } else { 32 }) ($W - $(if ($small) { 34 } else { 42 })) "body" 0 0 $false
    } elseif ($ln -match '^!\[(.*?)\]\((.+?)\)$') {
      $cap = $Matches[1]; $rel = $Matches[2]
      $script:y += 4
      $imgFile = Join-Path $Root ("sdcard\wiki\" + $rel.Replace("/", "\"))
      if (Test-Path $imgFile) {
        $im = [System.Drawing.Image]::FromFile($imgFile); $iw = $im.Width; $ih = $im.Height; $im.Dispose()
        $sc = 1; while (($iw / $sc) -gt $TW -and $sc -lt 8) { $sc *= 2 }
        $dh = [int]($ih / $sc); $dw = [int]($iw / $sc)
        Add-Item 4 0 0 ([int](($W - $dw) / 2)) $script:y $dw $dh ([System.Text.Encoding]::ASCII.GetBytes($rel))
        $script:y += $dh + 4
      } else { $script:y += 44 }
      if ($cap.Length -gt 0) { Add-Text $cap $TX $TW "cap" 2 0 $false }
      $script:y += 6
    } else {
      Add-Text $ln $TX $TW "body" 0 0 $false
    }
  }
  if ($inWarn) { Add-Item 1 0 1 0 $script:y $W 8 $null; $script:y += 8 }
  $H = $script:y + 16

  # file: "THP1", u16 W, u16 H, u16 count, u16 0, then items (16 bytes each), then the data
  $ms = New-Object System.IO.MemoryStream; $bw = New-Object System.IO.BinaryWriter($ms)
  $bw.Write([byte[]][char[]]"THP1"); $bw.Write([uint16]$W); $bw.Write([uint16]$H); $bw.Write([uint16]$items.Count); $bw.Write([uint16]0)
  $off = 12 + 16 * $items.Count
  foreach ($it in $items) {
    $bw.Write([byte]$it.type); $bw.Write([byte]$it.fg); $bw.Write([byte]$it.bg); $bw.Write([byte]0)
    $bw.Write([int16]$it.x); $bw.Write([int16]$it.y); $bw.Write([uint16]$it.w); $bw.Write([uint16]$it.h)
    if ($it.data -ne $null) { $bw.Write([uint32]$off); $off += $it.data.Length + $(if ($it.type -eq 4) { 1 } else { 0 }) } else { $bw.Write([uint32]0) }
  }
  foreach ($it in $items) { if ($it.data -ne $null) { $bw.Write([byte[]]$it.data); if ($it.type -eq 4) { $bw.Write([byte]0) } } }
  $bw.Flush()
  $dir = Join-Path $Out "p$board"; New-Item -ItemType Directory -Force $dir | Out-Null
  [IO.File]::WriteAllBytes((Join-Path $dir "$id.thp"), $ms.ToArray())
  return @{ H = $H; bytes = $ms.Length; items = $items }
}

foreach ($b in $Boards) {
  Build-Atlas $b
  $artDir = Join-Path $l10n "articles"
  if (Test-Path $artDir) {
    $n = 0; $tot = 0
    foreach ($f in (Get-ChildItem $artDir -Filter *.md)) {
      $r = Build-Page ($f.BaseName) $f.FullName $b
      $n++; $tot += $r.bytes
    }
    Write-Host ("pages {0}: {1} guides, {2:N0} KB" -f $b, $n, ($tot / 1KB))
  }
}
Write-Host "Done. Copy the folder '$Out' to the SD card root (it must end up as /th)."
