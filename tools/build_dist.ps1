<# Build a portable Windows test snapshot. Existing user settings are kept. #>
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
    & "$repo/src/build.ps1" -NoTest -StaticRuntime
    & "$repo/src/build.ps1" -Arch arm64 -NoTest -StaticRuntime
}
$lock = Get-Content -LiteralPath "$repo/docs/sources.lock.json" -Raw | ConvertFrom-Json
$version = (Get-Content -LiteralPath "$repo/src/VERSION" -Raw).Trim()
$commit = git -C $repo rev-parse HEAD
foreach ($arch in @('x64', 'arm64')) {
    $build = "$repo/work/build-$arch"
    $target = if ($arch -eq 'x64') { "$repo/dist" } else { "$repo/dist/arm64" }
    foreach ($folder in @($target, "$target/roms", "$target/docs/licenses")) {
        New-Item -ItemType Directory -Force -Path $folder | Out-Null
    }
    Publish-Program "$build/rec64.exe" "$target/rec64.exe"
    # Retired mapped images become removable after their process exits.
    Get-ChildItem -LiteralPath $target -Filter 'rec64.exe.running-*' -File | Where-Object {
        $_.Name -match '^rec64\.exe\.running-[0-9a-f]{32}$'
    } | ForEach-Object {
        try { Remove-Item -LiteralPath $_.FullName -Force -ErrorAction Stop } catch { }
    }
    if (-not (Test-Path -LiteralPath "$target/rec64.yaml")) {
        Copy-Item -LiteralPath "$repo/src/app/rec64.yaml" -Destination "$target/rec64.yaml"
    }
    # Remove only obsolete release palette files; user-owned files stay intact.
    foreach ($name in @('cjam.vpl', 'colodore.vpl', 'pepto-pal.vpl', 'pepto-ntsc.vpl')) {
        $obsolete = Join-Path $target "palettes/$name"
        if (Test-Path -LiteralPath $obsolete -PathType Leaf) { Remove-Item -LiteralPath $obsolete -Force }
    }
    $paletteFolder = Join-Path $target 'palettes'
    if ((Test-Path -LiteralPath $paletteFolder -PathType Container) -and
        -not (Get-ChildItem -LiteralPath $paletteFolder -Force | Select-Object -First 1)) {
        Remove-Item -LiteralPath $paletteFolder
    }
    foreach ($name in @('CONFIGURATION.md', 'THIRD_PARTY.md', 'LIMITATIONS.md', 'SID-COMPARISON.md', 'HARDWARE.md', 'LICENSE', 'sources.lock.json')) {
        Copy-Item -LiteralPath "$repo/docs/$name" -Destination "$target/docs/$name" -Force
    }
    Copy-Item -LiteralPath "$repo/docs/TESTING-BUILD.md" -Destination "$target/README.md" -Force
    Copy-Item -LiteralPath "$repo/src/third_party/libyaml/LICENSE" -Destination "$target/docs/licenses/libyaml-MIT" -Force
    Copy-Item -LiteralPath "$repo/docs/licenses/reSID-GPL-2" -Destination "$target/docs/licenses/reSID-GPL-2" -Force
    Copy-Item -LiteralPath "$build/_deps/sdl2-src/LICENSE.txt" -Destination "$target/docs/licenses/SDL-zlib" -Force
    foreach ($rom in $lock.roms) {
        $source = "$repo/roms/$($rom.file)"
        if ((Get-Item -LiteralPath $source).Length -ne $rom.bytes -or
            (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $rom.sha256) {
            throw "Firmware differs from the source lock: $($rom.file)"
        }
        Copy-Item -LiteralPath $source -Destination "$target/roms/$($rom.file)" -Force
    }
    $manifest = [ordered]@{
        version = $version; architecture = "windows-$arch"; commit = $commit
        executable_sha256 = (Get-FileHash -LiteralPath "$target/rec64.exe" -Algorithm SHA256).Hash.ToLowerInvariant()
        firmware = $lock.roms
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$target/docs/build.json" -Encoding utf8
    Write-Host "Ready: $target/rec64.exe ($version, $arch)"
}
