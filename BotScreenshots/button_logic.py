import pyautogui
import pickle

# Datei, um die Button-Position zu speichern
learning_model_file = "button_position.pkl"

# Variable zur Speicherung der Button-Position
button_position = None

# Funktion zur Suche nach dem Button und Klick
def click_button_if_found(button_image_path):
    global button_position

    # Versuche, die gespeicherte Position zuerst zu verwenden
    if button_position:
        print(f"Versuche, Button an der gespeicherten Position {button_position} zu finden...")
        pyautogui.moveTo(button_position, duration=0.5)  # Bewege die Maus langsam zur gespeicherten Position
        print(f"Maus wurde zur gespeicherten Position {button_position} bewegt.")
        pyautogui.click()  # Klicke auf den Button
        print("Button wurde angeklickt!")  # Sichtbare Ausgabe für den Klick

        # Frage nach Feedback, ob der Klick korrekt war
        feedback = input("War der Button an der gespeicherten Position korrekt? (y/n): ").lower()
        if feedback == "y":
            print("Button korrekt an der gespeicherten Position gefunden!")  # Bestätige den erfolgreichen Klick
            return True
        else:
            print("Button hat seine Position verändert, suche dynamisch...")

    # Dynamische Suche nach dem Button, falls die gespeicherte Position nicht korrekt war
    try:
        print(f"Suche nach dem Button mit Bild: {button_image_path}")
        button_location = pyautogui.locateCenterOnScreen(button_image_path)
        if button_location is not None:
            print(f"Button bei {button_location} gefunden!")
            pyautogui.moveTo(button_location, duration=0.5)  # Bewege die Maus langsam zur neuen Position
            print(f"Maus wurde zur dynamischen Position {button_location} bewegt.")
            pyautogui.click()  # Klicke auf den Button
            print(f"Button bei {button_location} dynamisch gefunden und geklickt!")  # Sichtbare Ausgabe für den Klick

            # Feedback erneut erfragen
            feedback = input("War der Button-Klick korrekt? (y/n): ").lower()
            if feedback == "y":
                print("Speichere die neue Position des Buttons.")
                button_position = button_location  # Speichere die neue Position
                with open(learning_model_file, 'wb') as f:
                    pickle.dump(button_position, f)  # Speichere die Position in einer Datei
            else:
                print("Button wurde nicht korrekt erkannt.")
            return True
        else:
            print("Button nicht gefunden. Überprüfe das Button-Bild.")
    except Exception as e:
        print(f"Fehler beim Suchen des Buttons: {e}")
    
    return False
