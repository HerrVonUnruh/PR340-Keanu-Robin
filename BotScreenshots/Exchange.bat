@echo off
set "filepath=C:\Users\robin\Desktop\BotScreenshots"
set "filetxt=%filepath%\bot.txt"
set "filepy=%filepath%\bot.py"

if exist "%filetxt%" (
    ren "%filetxt%" bot.py
    echo bot.txt wurde in bot.py umbenannt.
) else (
    if exist "%filepy%" (
        ren "%filepy%" bot.txt
        echo bot.py wurde in bot.txt umbenannt.
    ) else (
        echo Weder bot.txt noch bot.py existiert.
    )
)

exit
