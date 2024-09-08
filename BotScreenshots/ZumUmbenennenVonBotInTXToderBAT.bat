@echo off
set "filepath=C:\Users\robin\Desktop\4. Semester\Pruefungen\PR340_Netzwerkpruefung\PR340-Keanu-Robin\BotScreenshots"
set "filetxt=%filepath%\bot.txt"
set "filepy=%filepath%\bot.py"

:: Debugging: Ausgabe des Dateipfads und der Dateien
echo Filepath: %filepath%
echo Überprüfe Existenz von bot.txt: %filetxt%
echo Überprüfe Existenz von bot.py: %filepy%

:: Prüfen, ob bot.txt existiert
if exist "%filetxt%" (
    echo bot.txt existiert.
    ren "%filetxt%" bot.py
    if errorlevel 1 (
        echo Fehler beim Umbenennen von bot.txt in bot.py.
    ) else (
        echo bot.txt wurde in bot.py umbenannt.
    )
) else (
    :: Prüfen, ob bot.py existiert
    if exist "%filepy%" (
        echo bot.py existiert.
        ren "%filepy%" bot.txt
        if errorlevel 1 (
            echo Fehler beim Umbenennen von bot.py in bot.txt.
        ) else (
            echo bot.py wurde in bot.txt umbenannt.
        )
    ) else (
        echo Weder bot.txt noch bot.py existiert.
    )
)

exit
