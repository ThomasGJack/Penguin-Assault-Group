# Saisie du jeton du bot sur le VPS, en masqué. Le jeton ne passe ni par un fichier de ce PC, ni par un argument, ni par Claude.
$hote = (Get-Content (Join-Path $PSScriptRoot "vps\hote.txt") -Raw).Trim()
$cible, $port = $hote -split ":"
$cle = Join-Path $env:USERPROFILE ".ssh\pag_vps"
Write-Host "Connexion à $cible. Colle le jeton quand il est demandé (rien ne s'affiche pendant la saisie), puis Entrée." -ForegroundColor Cyan
ssh -t -i $cle -p $port $cible "sudo pag-bot-jeton"
