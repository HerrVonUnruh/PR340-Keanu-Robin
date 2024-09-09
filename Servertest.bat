@echo off
setlocal enabledelayedexpansion

:: Löschen von .obj und .exe Dateien zu Beginn
echo Lösche bestehende .obj und .exe Dateien...
if exist *.obj del *.obj
if exist *.exe del *.exe

:: Pfad zur Textdatei festlegen
set "path_file=%~dp0script_paths.txt"

:load_paths
:: Überprüfen, ob die Textdatei existiert und gültige Pfade enthält
if exist "%path_file%" (
    echo Lade Pfade aus der Textdatei...
    for /f "tokens=*" %%a in ('more +0 "%path_file%"') do (
        if not defined client_base_path (
            set "client_base_path=%%a"
        ) else if not defined server_base_path (
            set "server_base_path=%%a"
        )
    )
    
    echo Geladene Pfade:
    echo Client-Pfad: !client_base_path!
    echo Server-Pfad: !server_base_path!
    
    :: Frage den Benutzer, ob die Pfade korrekt sind
    set /p "confirm=Sind diese Pfade korrekt? (J/N): "
    if /i "!confirm!"=="N" goto :ask_paths
)

:: Wenn die Datei nicht existiert oder ungültige Pfade enthält, frage nach den Pfaden
if not exist "%path_file%" goto :ask_paths

goto :check_paths

:ask_paths
echo Bitte gib die korrekten Pfade zu den Skripten ein.
set /p "client_base_path=Basis-Pfad des Client-Projekts (z.B. C:\Projekte\Client): "
set /p "server_base_path=Basis-Pfad des Server-Projekts (z.B. C:\Projekte\Server): "

:: Speichere die Basis-Pfade in der Textdatei
echo Speichere Pfade in der Textdatei...
(
    echo !client_base_path!
    echo !server_base_path!
) > "%path_file%"
echo Pfade erfolgreich gespeichert.

:check_paths
:: Kombiniere die Basis-Pfade mit den Dateinamen
set "client_full_path=!client_base_path!\client.cpp"
set "server_full_path=!server_base_path!\server.cpp"

:: Überprüfe, ob die Dateien existieren
if not exist "!client_full_path!" (
    echo Fehler: Client-Datei nicht gefunden: !client_full_path!
    goto :ask_paths
)
if not exist "!server_full_path!" (
    echo Fehler: Server-Datei nicht gefunden: !server_full_path!
    goto :ask_paths
)

:: Debug-Ausgabe der kombinierten Pfade für Client und Server
echo Verwende Pfade:
echo Client Pfad: !client_full_path!
echo Server Pfad: !server_full_path!

:: Pfad zur vcvarsall.bat Datei (Visual Studio Umgebung einrichten)
set "VS_PATH="
for %%V in (2022 2019 2017) do (
    if exist "C:\Program Files\Microsoft Visual Studio\%%V\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VS_PATH=C:\Program Files\Microsoft Visual Studio\%%V\Community\VC\Auxiliary\Build\vcvarsall.bat"
        goto :found_vs
    )
)
:found_vs

if not defined VS_PATH (
    echo Fehler: Visual Studio nicht gefunden. Bitte installieren Sie Visual Studio.
    goto :error
)

call "!VS_PATH!" x64

:: Kompilieren und Ausführen des Servers
echo Kompiliere und starte den Server...
cl "!server_full_path!" /Fe:server.exe && start "Server" cmd /k server.exe

:: Kompilieren und Ausführen der Clients
echo Kompiliere und starte den ersten Client...
cl "!client_full_path!" /Fe:client1.exe && start "Client 1" cmd /k client1.exe

echo Kompiliere und starte den zweiten Client...
cl "!client_full_path!" /Fe:client2.exe && start "Client 2" cmd /k client2.exe

echo.
echo Alle Programme wurden gestartet. Sie laufen jetzt in separaten Fenstern.
echo Dieses Fenster bleibt geöffnet, damit Sie die Ausgabe sehen können.
echo.
echo Drücken Sie eine beliebige Taste, um aufzuräumen und das Skript zu beenden...
goto :end

:error
echo Ein Fehler ist aufgetreten. Das Skript wird beendet.

:end
pause

:: Aufräumen
echo Lösche temporäre Dateien...
if exist *.obj del *.obj

echo Skript beendet.
