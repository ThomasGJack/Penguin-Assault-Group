# Lance PAG-Bot et le relance s'il s'arrete. Le jeton vient des variables d'environnement du compte
# (enregistre par installer.bat) ; sinon il est demande en saisie masquee et ne vit que dans ce processus.
param([switch]$Silencieux)
$ErrorActionPreference = "Continue"
Set-Location $PSScriptRoot
$env:PYTHONUTF8 = "1"

if (-not (Test-Path "venv\Scripts\python.exe") -or -not (Test-Path "config.json")) {
    Write-Host "Installation absente : lance d'abord installer.bat" -ForegroundColor Red
    if (-not $Silencieux) { pause }
    exit 1
}

if (-not $env:DISCORD_TOKEN) { $env:DISCORD_TOKEN = [Environment]::GetEnvironmentVariable("DISCORD_TOKEN", "User") }
if (-not $env:DISCORD_TOKEN) {
    if ($Silencieux) { exit 1 }
    $secure = Read-Host "Jeton du bot (saisie masquee, colle-le puis Entree)" -AsSecureString
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    $env:DISCORD_TOKEN = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
    if (-not $env:DISCORD_TOKEN) { Write-Host "Jeton vide, abandon."; exit 1 }
}

while ($true) {
    if ($Silencieux) {
        & "venv\Scripts\python.exe" bot.py *>> "pag-bot.log"
    } else {
        & "venv\Scripts\python.exe" bot.py
    }
    Write-Host "Le bot s'est arrete (code $LASTEXITCODE). Relance dans 15 s, Ctrl+C pour quitter."
    Start-Sleep -Seconds 15
}
