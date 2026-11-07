param([Parameter(Mandatory=$true)][string]$ToolchainBin,[switch]$VerifyReference)
$ErrorActionPreference = 'Stop'
$clang = Join-Path $ToolchainBin 'clang.exe'
$lld = Join-Path $ToolchainBin 'ld.lld.exe'
if (!(Test-Path -LiteralPath $clang -PathType Leaf) -or !(Test-Path -LiteralPath $lld -PathType Leaf)) { throw 'Pass the bin directory of an LLVM-MinGW toolchain.' }
$dest = Join-Path $PSScriptRoot 'build'
[IO.Directory]::CreateDirectory($dest) | Out-Null
$images = @(
    @{Source='PB60-EFI-Diagnostic'; Target='PB60DIAG'; Timestamp=1791961644; Reference='338a938364a1ddfffe412999a5c27e0ff04f21a08d0fc309ffd417f172003c59'},
    @{Source='PB60-Custom-Mode-Helper'; Target='PB60MODE'; Timestamp=0; Reference='45e8693e5bf57085d405e00106c0edb82262e5b8255cd35d412910bdeba50809'}
)
foreach ($item in $images) {
    $source = Join-Path $PSScriptRoot ('src\' + $item.Source + '.c')
    $obj = Join-Path $dest ($item.Target + '.obj')
    $image = Join-Path $dest ($item.Target + '.EFI')
    & $clang --target=x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone -O1 -Wall -Wextra -Werror -c $source -o $obj
    if ($LASTEXITCODE) { throw "clang failed: $($item.Source)" }
    & $lld -flavor link /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x64 "/timestamp:$($item.Timestamp)" "/out:$image" $obj
    if ($LASTEXITCODE) { throw "lld failed: $($item.Source)" }
    $hash = (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Output "$($item.Target).EFI  $hash"
    if ($VerifyReference -and $hash -ine $item.Reference) { throw "Reference image mismatch: $($item.Target).EFI" }
}
