# Arrête le PAG-Bot de ce PC et supprime son démarrage automatique (à faire une fois le bot installé sur le VPS).
# Les fichiers restent en place : relancer installer.bat suffit pour revenir en arrière.
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
    # Supprimer une tâche planifiée demande les droits administrateur : on se relance avec (fenêtre de confirmation Windows)
    Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -NoExit -File `"$PSCommandPath`""
    exit
}
$ErrorActionPreference = "SilentlyContinue"
$tache = Get-ScheduledTask -TaskName "PAG-Bot"
if ($tache) {
    Stop-ScheduledTask -TaskName "PAG-Bot"
    Unregister-ScheduledTask -TaskName "PAG-Bot" -Confirm:$false
    if (Get-ScheduledTask -TaskName "PAG-Bot") { Write-Host "La tâche planifiée PAG-Bot n'a PAS pu être supprimée." -ForegroundColor Red } else { Write-Host "Démarrage automatique supprimé (tâche planifiée PAG-Bot)." -ForegroundColor Green }
} else {
    Write-Host "Pas de tâche planifiée PAG-Bot."
}
$n = 0
Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like "*PAG-Bot*" -and ($_.Name -eq "python.exe" -or ($_.Name -eq "powershell.exe" -and $_.CommandLine -like "*lancer.ps1*")) } | ForEach-Object {
    Stop-Process -Id $_.ProcessId -Force
    $n++
}
Write-Host "$n processus du bot arrêté(s) sur ce PC."
Write-Host "Le bot ne tourne plus ici. Celui du VPS prend le relais."
