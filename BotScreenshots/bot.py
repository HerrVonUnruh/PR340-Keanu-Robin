import pyautogui
import time
from PIL import Image
import os
import json
import subprocess  # Für das Starten des Snipping Tools
import threading

# Dateipfade
config_file = "config.json"
button_image_path = r"C:\Users\robin\Desktop\BotScreenshots\button_image.png"
arrow_button_image_path = r"C:\Users\robin\Desktop\BotScreenshots\PfeilButton.png"

# Lade oder erstelle Screenshot-Ordner
def get_screenshot_folder():
    if os.path.exists(config_file):
        with open(config_file, 'r') as f:
            config = json.load(f)
            return config.get("screenshot_folder")
    else:
        folder = input("Bitte gib den Dateipfad für die Screenshots ein: ")
        with open(config_file, 'w') as f:
            json.dump({"screenshot_folder": folder}, f)
        return folder

# Screenshots speichern und löschen
def take_screenshot(filename, folder):
    screenshot = pyautogui.screenshot()
    screenshot.save(os.path.join(folder, filename))
    return os.path.join(folder, filename)

def safe_delete(filename, folder):
    path = os.path.join(folder, filename)
    if os.path.exists(path):
        os.remove(path)

# Buttons finden und klicken
def find_and_click_button(button_image, screenshot_name, folder):
    try:
        button_location = pyautogui.locateCenterOnScreen(button_image)
        if button_location:
            pyautogui.moveTo(button_location, duration=0.1)
            time.sleep(0.1)
            pyautogui.click()
            return True
        else:
            return False  # Button wurde nicht gefunden, aber kein Fehler notwendig
    except Exception as e:
        print(f"Fehler beim Suchen des Buttons: {e}")
        return False

# Funktion zur Aktivierung des Snipping Tools
def activate_snipping_tool():
    print("Snipping Tool wird gestartet. Bitte erstelle einen Snip vom Button.")
    subprocess.run("snippingtool", shell=True)  # Snipping Tool wird gestartet

# Funktion zur Feedback-Abfrage nach einem Klick oder Snip
def ask_for_feedback():
    feedback = input("War der Klick korrekt? (j für Ja, n für Nein): ").lower()
    if feedback == 'n':
        missed = input("Wurde etwas übersehen? (j für Ja, n für Nein): ").lower()
        if missed == 'j':
            button_type = input("War es der (a) Generation Button oder (b) PfeilButton?: ").lower()
            if button_type == 'a':
                print("Lerne über den Generation Button.")
            elif button_type == 'b':
                print("Lerne über den PfeilButton.")
            activate_snipping_tool()
            # Der Bot lernt jetzt aus dem Snip, kein Bild bleibt gespeichert
            print("Bot hat aus dem neuen Screenshot gelernt.")
        else:
            print("Programm wird beendet.")

# Screenshot-Schleife mit Tasteneingabe-Unterbrechung
def screenshot_loop(folder):
    global running_screenshot_loop
    while running_screenshot_loop:
        if wait_for_input(5):
            print("Tastatureingabe erkannt. Frage, ob etwas übersehen wurde.")
            ask_for_feedback()
            return  # Beendet die Schleife und kehrt ins Hauptmenü zurück

        # Screenshot 1 machen
        screenshot_1 = take_screenshot("Screenshot_1.png", folder)
        found_button = find_and_click_button(button_image_path, "Screenshot_1.png", folder)
        found_arrow_button = find_and_click_button(arrow_button_image_path, "Screenshot_1.png", folder)

        if found_button or found_arrow_button:
            print("Button gefunden und geklickt. Warte 5 Sekunden.")
            time.sleep(5)
            screenshot_2 = take_screenshot("Screenshot_2.png", folder)
            
            button_still_present = find_and_click_button(button_image_path, "Screenshot_2.png", folder)
            arrow_button_still_present = find_and_click_button(arrow_button_image_path, "Screenshot_2.png", folder)
            
            if not button_still_present and not arrow_button_still_present:
                safe_delete("Screenshot_1.png", folder)
                safe_delete("Screenshot_2.png", folder)
            else:
                print("Button immer noch vorhanden.")
        else:
            safe_delete("Screenshot_1.png", folder)

        time.sleep(5)

# Funktion, um die Schleife durch Tasteneingabe zu unterbrechen
def wait_for_input(timeout=5):
    result = [False]
    
    def check_input():
        input("Drücke eine Taste, um das Programm zu unterbrechen und ins Hauptmenü zurückzukehren...\n")
        result[0] = True
    
    thread = threading.Thread(target=check_input)
    thread.daemon = True
    thread.start()
    thread.join(timeout)
    
    return result[0]

# Hauptlogik des Bots
def run_bot():
    folder = get_screenshot_folder()
    while True:
        choice = input("Drücke 'x' zum Suchen, 'y' für die Screenshot-Schleife, '3' zum Beenden: ").lower()
        if choice == 'x':
            # X-Modus: Suchen und Feedback einholen
            found_button = find_and_click_button(button_image_path, "Screenshot_1.png", folder)
            found_arrow_button = find_and_click_button(arrow_button_image_path, "Screenshot_1.png", folder)
            if found_button or found_arrow_button:
                ask_for_feedback()  # Feedback nur abfragen, wenn einer der Buttons geklickt wurde
        elif choice == 'y':
            global running_screenshot_loop
            running_screenshot_loop = True
            screenshot_loop(folder)
        elif choice == '3':
            print("Programm beendet.")
            break

if __name__ == "__main__":
    run_bot()
