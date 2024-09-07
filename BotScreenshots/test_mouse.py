import pyautogui
import time

# Warte 3 Sekunden, um das Skript zu starten
print("Das Skript startet in 3 Sekunden...")
time.sleep(3)

# Hole die Bildschirmgröße
screen_width, screen_height = pyautogui.size()

# Bewege die Maus in die Mitte des Hauptbildschirms
middle_x = screen_width // 2
middle_y = screen_height // 2

print(f"Bewege die Maus zur Mitte des Bildschirms: ({middle_x}, {middle_y})")
pyautogui.moveTo(middle_x, middle_y, duration=1)  # Bewege die Maus zur Mitte des Bildschirms

# Führe einen linken Mausklick aus
print("Führe einen linken Mausklick aus...")
pyautogui.click()

print("Maus wurde bewegt und geklickt.")
