# Builds the Thai SD card pack: the English card (sdcard/) with the Thai guides, titles, search index, diagrams and
# books from l10n/th/sdcard/ laid over it. The PDFs are not kept in the repo; pass the folder that has them.
# Run:  powershell -File scripts/thai/build-thai-sdcard.ps1 -Out C:\somewhere\sdcard-thai [-Pdfs <folder with field-guide.pdf and fm21-76.pdf>] [-Zip file.zip]
param([Parameter(Mandatory)][string]$Out, [string]$Pdfs = '', [string]$Zip = '')
$root = Split-Path (Split-Path $PSScriptRoot)
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory -Force $Out | Out-Null
Copy-Item "$root\sdcard\*" $Out -Recurse -Force
Copy-Item "$root\l10n\th\sdcard\*" $Out -Recurse -Force
Get-ChildItem "$Out\library\field-guide\*.txt" | Where-Object { -not (Test-Path "$root\l10n\th\sdcard\library\field-guide\$($_.Name)") } | Remove-Item
if ($Pdfs) {
  Copy-Item "$Pdfs\field-guide.pdf" "$Out\library\field-guide\field-guide.pdf" -Force
  Copy-Item "$Pdfs\fm21-76.pdf" "$Out\library\fm21-76\fm21-76.pdf" -Force
}
if ($Zip) {
  if (Test-Path $Zip) { Remove-Item $Zip }
  Compress-Archive -Path "$Out\*" -DestinationPath $Zip
}
"$((Get-ChildItem $Out -Recurse -File | Measure-Object).Count) files in $Out"