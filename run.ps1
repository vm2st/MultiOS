# vm2 studios | t.me/v2vproj - MultiOS / MultiPulti
# Run from PowerShell: powershell -ExecutionPolicy Bypass -File .\run.ps1
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
$qemu = Get-Command qemu-system-x86_64.exe -ErrorAction SilentlyContinue
if ($qemu) { $exe = $qemu.Source } else { $exe = "$env:ProgramFiles\qemu\qemu-system-x86_64.exe" }
if (!(Test-Path $exe)) { throw "Install QEMU for Windows or add it to PATH." }
if (!(Test-Path build\work.img)) { Copy-Item build\multios.img build\work.img }
& $exe -machine pc -cpu qemu64 -m 32M -drive "file=build/work.img,format=raw,if=ide,index=0" -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04
if ($LASTEXITCODE -eq 33) { exit 0 }
exit $LASTEXITCODE
