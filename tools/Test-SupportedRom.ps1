param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath
)

$ErrorActionPreference = 'Stop'
$expectedSha256 = '751ABB1FB2B2D6B91DC631FA10ED36BD07F682CB5C3FEBE6C8F0E3DABC1FAE1C'
$expectedBytes = 524288
$expectedRevision = 1

$resolved = (Resolve-Path -LiteralPath $RomPath).Path
$bytes = [IO.File]::ReadAllBytes($resolved)
if ($bytes.Length -lt 0x150) {
    throw 'The file is too short to contain a Game Boy cartridge header.'
}

$headerChecksum = 0
foreach ($offset in 0x134..0x14C) {
    $headerChecksum = ($headerChecksum - $bytes[$offset] - 1) -band 255
}

$globalChecksum = 0
for ($offset = 0; $offset -lt $bytes.Length; $offset++) {
    if ($offset -ne 0x14E -and $offset -ne 0x14F) {
        $globalChecksum = ($globalChecksum + $bytes[$offset]) -band 65535
    }
}

$sha256 = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash
$revision = [int]$bytes[0x14C]
$report = [ordered]@{
    supported = $sha256 -eq $expectedSha256 -and
        $bytes.Length -eq $expectedBytes -and
        $revision -eq $expectedRevision -and
        $headerChecksum -eq $bytes[0x14D] -and
        $globalChecksum -eq (([int]$bytes[0x14E] -shl 8) -bor $bytes[0x14F])
    path = $resolved
    title = [Text.Encoding]::ASCII.GetString($bytes, 0x134, 16).TrimEnd([char]0)
    bytes = $bytes.Length
    sha256 = $sha256
    revision = $revision
    headerChecksumValid = $headerChecksum -eq $bytes[0x14D]
    globalChecksumValid = $globalChecksum -eq
        (([int]$bytes[0x14E] -shl 8) -bor $bytes[0x14F])
}

$report | ConvertTo-Json
if (-not $report.supported) {
    Write-Error 'This ROM does not match the version supported by the current payload.'
    exit 1
}
