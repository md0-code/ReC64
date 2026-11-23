<# Build the portable Windows x64 folder (dist/; existing user settings, ROMs and other files are kept) and the release
   archives dist/releases/ReC64-<version>-windows-x64.zip and -windows-arm64.zip (tools/package.py), which hold no
   firmware. dist/releases/ holds the current version only. #>
[CmdletBinding()]
param([switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath("$PSScriptRoot/..")
function Publish-Program([string]$source, [string]$destination) {
    if ((Test-Path -LiteralPath $destination) -and
        (Get-FileHash -LiteralPath $source).Hash -eq (Get-FileHash -LiteralPath $destination).Hash) { return }
    try { Copy-Item -LiteralPath $source -Destination $destination -Force; return }
    catch {
        $publishError = $_
        if (-not (Test-Path -LiteralPath $destination)) { throw }
        # Windows permits renaming a mapped image while its process continues.
        $retained = $destination + '.running-' + [Guid]::NewGuid().ToString('N')
        try { Move-Item -LiteralPath $destination -Destination $retained }
        catch { throw $publishError }
        try { Copy-Item -LiteralPath $source -Destination $destination -Force }
        catch {
            if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Force }
            Move-Item -LiteralPath $retained -Destination $destination
            throw
        }
        try { Remove-Item -LiteralPath $retained -Force -ErrorAction Stop }
        catch { Write-Host "Preserved running image: $retained" }
    }
}
if (-not $NoBuild) {
    & "$repo/tools/build.ps1" -NoTest -StaticRuntime -Pgo
    & "$repo/tools/build.ps1" -Arch arm64 -NoTest -StaticRuntime
}
$version = (Get-Content -LiteralPath "$repo/src/VERSION" -Raw).Trim()

# The run folder: the x64 program and the documents of the release.
$target = "$repo/dist"
foreach ($folder in @($target, "$target/roms", "$target/docs", "$target/cores", "$target/info")) {
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
}
Publish-Program "$repo/build/x64/rec64.exe" "$target/rec64.exe"
Publish-Program "$repo/build/x64/rec64_libretro.dll" "$target/cores/rec64_libretro.dll"
Copy-Item -LiteralPath "$repo/src/libretro/rec64_libretro.info" -Destination "$target/info/rec64_libretro.info" -Force
# Retired mapped images become removable after their process exits.
Get-ChildItem -LiteralPath $target -Filter 'rec64.exe.running-*' -File | Where-Object {
    $_.Name -match '^rec64\.exe\.running-[0-9a-f]{32}$'
} | ForEach-Object {
    try { Remove-Item -LiteralPath $_.FullName -Force -ErrorAction Stop } catch { }
}
if (-not (Test-Path -LiteralPath "$target/rec64.yaml")) {
    Copy-Item -LiteralPath "$repo/src/app/rec64.yaml" -Destination "$target/rec64.yaml"
}
foreach ($name in @('NOTES.md', 'CHANGELOG.md', 'THIRD_PARTY.md', 'LICENSE')) {
    Copy-Item -LiteralPath "$repo/docs/$name" -Destination "$target/docs/$name" -Force
}
Copy-Item -LiteralPath "$repo/README.md" -Destination "$target/README.md" -Force
# What earlier releases put in the run folder and this one does not.
foreach ($old in @("$target/docs/DESIGN.md", "$target/docs/build.json", "$target/docs/licenses", "$target/arm64")) {
    if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Recurse -Force }
}
Write-Host "Ready: $target/rec64.exe ($version, x64)"

# The release archives: the same program and documents, no firmware and no settings.
foreach ($arch in @('x64', 'arm64')) {
    python "$repo/tools/package.py" "windows-$arch" "$repo/build/$arch/rec64.exe" --out "$repo/dist/releases"
    if ($LASTEXITCODE -ne 0) { throw "packaging windows-$arch failed" }
}
