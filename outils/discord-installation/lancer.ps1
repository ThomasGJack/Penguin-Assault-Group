# Lance l'installation du Discord SimpleRP.
# Le jeton est demande en saisie masquee et ne vit que dans ce processus : jamais ecrit sur le disque.
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$guild = Read-Host "Identifiant du serveur Discord (Entree = 1548785514080108574)"
if (-not $guild) { $guild = "1548785514080108574" }

$secure = Read-Host "Jeton du bot (saisie masquee, colle-le puis Entree)" -AsSecureString
$ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
$token = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
if (-not $token) { Write-Host "Jeton vide, abandon."; exit 1 }

$env:DISCORD_TOKEN = $token
$env:DISCORD_GUILD = $guild.Trim()

python -m pip show requests *> $null
if ($LASTEXITCODE -ne 0) { python -m pip install requests }

python "$PSScriptRoot\setup_discord.py"

$env:DISCORD_TOKEN = $null
Write-Host ""
Write-Host "Termine. webhooks.json est a cote du script : il contient des URL secretes, ne le partage pas."
