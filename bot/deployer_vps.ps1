# PAG-Bot : envoie le bot sur le VPS Debian et lance l'installation. Rien n'est stocké : ni mot de passe, ni jeton.
#   - le mot de passe (ou la clé) du VPS est demandé par ssh lui-même ;
#   - le jeton du bot est demandé par le script d'installation, sur le VPS, en saisie masquée.
# Usage : double-clic sur deployer_vps.bat, ou  .\deployer_vps.ps1 -Ip 1.2.3.4 [-Utilisateur debian] [-Port 22] [-Jeton]
param(
    [string]$Ip,
    [string]$Utilisateur = "debian",
    [int]$Port = 22,
    [switch]$Jeton      # redemander le jeton sur le VPS (après un Reset Token)
)
$ErrorActionPreference = "Stop"
$ici = $PSScriptRoot

if (-not (Get-Command ssh -ErrorAction SilentlyContinue) -or -not (Get-Command scp -ErrorAction SilentlyContinue)) {
    Write-Host "Le client OpenSSH de Windows est absent. Installe-le : Paramètres > Applications > Fonctionnalités facultatives > Client OpenSSH." -ForegroundColor Red
    exit 1
}
if (-not $Ip) { $Ip = Read-Host "Adresse IP du VPS" }
$Ip = $Ip.Trim()
if (-not $Ip) { Write-Host "Pas d'adresse, abandon." -ForegroundColor Red; exit 1 }

# Dossier d'envoi temporaire : uniquement ce dont le VPS a besoin (pas le venv Windows, pas les journaux)
$envoi = Join-Path $env:TEMP "pag-bot-envoi"
if (Test-Path $envoi) { Remove-Item -Recurse -Force $envoi }
New-Item -ItemType Directory -Path $envoi | Out-Null
foreach ($f in @("bot.py", "medical.py", "suivi.py", "campagne.py", "trace_image.py", "requirements.txt", "config.json")) { Copy-Item (Join-Path $ici $f) $envoi }
if (Test-Path (Join-Path $ici "state.json")) { Copy-Item (Join-Path $ici "state.json") $envoi }
Copy-Item (Join-Path $ici "vps\installer_vps.sh") $envoi

$cible = "$Utilisateur@$Ip"
$cle = Join-Path $env:USERPROFILE ".ssh\pag_vps"
$sshOpt = @()
if (Test-Path $cle) { $sshOpt = @("-i", $cle) }
Write-Host ""
Write-Host "1/2  Envoi des fichiers vers $cible (ssh va demander le mot de passe du VPS)" -ForegroundColor Cyan
ssh @sshOpt -p $Port $cible "rm -rf ~/pag-bot-install && mkdir -p ~/pag-bot-install"
if ($LASTEXITCODE -ne 0) { Write-Host "Connexion impossible. Vérifie l'adresse, l'utilisateur et le mot de passe." -ForegroundColor Red; exit 1 }
scp @sshOpt -P $Port -q "$envoi\*" "${cible}:pag-bot-install/"
if ($LASTEXITCODE -ne 0) { Write-Host "L'envoi a échoué." -ForegroundColor Red; exit 1 }
Remove-Item -Recurse -Force $envoi

Write-Host ""
Write-Host "2/2  Installation sur le VPS" -ForegroundColor Cyan
$option = ""
if ($Jeton) { $option = " --jeton" }
$sudo = ""
if ($Utilisateur -ne "root") { $sudo = "sudo " }
# -t : un vrai terminal, indispensable pour la saisie masquée du jeton
ssh @sshOpt -t -p $Port $cible "${sudo}bash ~/pag-bot-install/installer_vps.sh$option; code=`$?; rm -rf ~/pag-bot-install; exit `$code"
if ($LASTEXITCODE -ne 0) { Write-Host "L'installation a signalé une erreur, lis les lignes ci-dessus." -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "Terminé. Il reste deux choses à faire :" -ForegroundColor Green
Write-Host "  1. Sur LobbyHost, dans profile/SimpleRP/discord.json, remplace bridge_url par l'adresse affichée ci-dessus, puis redémarre le serveur de jeu."
Write-Host "  2. Arrête le bot de ce PC : lance arreter_bot_local.bat (deux bots avec le même jeton se marchent dessus)."
