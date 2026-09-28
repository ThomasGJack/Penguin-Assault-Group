# Autorise la clé SSH dédiée (C:\Users\<toi>\.ssh\pag_vps) sur le VPS. Le mot de passe du VPS est demandé par ssh, une seule fois.
param([string]$Ip, [string]$Utilisateur = "debian", [int]$Port = 22)
$pub = Join-Path $env:USERPROFILE ".ssh\pag_vps.pub"
if (-not (Test-Path $pub)) { Write-Host "Clé publique introuvable : $pub" -ForegroundColor Red; exit 1 }
if (-not $Ip) { $Ip = Read-Host "Adresse IP du VPS" }
$Ip = $Ip.Trim()
Write-Host "ssh va demander le mot de passe du VPS (une seule fois)." -ForegroundColor Cyan
Get-Content $pub | ssh -p $Port "$Utilisateur@$Ip" "mkdir -p ~/.ssh && chmod 700 ~/.ssh && cat >> ~/.ssh/authorized_keys && sort -u ~/.ssh/authorized_keys -o ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys && echo CLE-AUTORISEE"
if ($LASTEXITCODE -ne 0) { Write-Host "Échec. Vérifie l'adresse et le mot de passe." -ForegroundColor Red; exit 1 }
Write-Host ""
Write-Host "Test de la connexion par clé, sans mot de passe :" -ForegroundColor Cyan
ssh -i (Join-Path $env:USERPROFILE ".ssh\pag_vps") -o BatchMode=yes -o StrictHostKeyChecking=accept-new -p $Port "$Utilisateur@$Ip" "echo CONNEXION-PAR-CLE-OK ; hostname ; cat /etc/debian_version"
Set-Content -Path (Join-Path $PSScriptRoot "vps\hote.txt") -Value "$Utilisateur@$Ip`:$Port" -Encoding ascii
Write-Host ""
Write-Host "C'est bon. Dis à Claude que la clé est en place." -ForegroundColor Green
