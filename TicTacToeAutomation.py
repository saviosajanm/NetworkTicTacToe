import pyautogui
import time

pyautogui.FAILSAFE = True

def open_network_tic_tac_toe():
    pyautogui.press('win')
    time.sleep(0.5)
    pyautogui.typewrite("NetworkTicTacToe", interval=0.05)
    time.sleep(0.5)
    pyautogui.press('enter')
    time.sleep(0.5)

# Step 1: Open both apps
open_network_tic_tac_toe()
time.sleep(1)
open_network_tic_tac_toe()
time.sleep(1)

# Step 2: Arrange side-by-side
pyautogui.hotkey('alt', 'tab')  # first window
time.sleep(0.5)
pyautogui.hotkey('win', 'left')
time.sleep(0.5)
pyautogui.press('enter')

# Step 3: Commands for first TicTacToe
pyautogui.hotkey('alt', 'tab')
time.sleep(0.5)
pyautogui.press('alt')
pyautogui.press('n')
pyautogui.press('x')
time.sleep(0.3)
pyautogui.press('alt')
pyautogui.press('n')
pyautogui.press(['down', 'down', 'down'])
pyautogui.press('enter')

# Step 4: Commands for second TicTacToe
pyautogui.hotkey('alt', 'tab')
time.sleep(0.5)
pyautogui.press('alt')
pyautogui.press('n')
pyautogui.press('o')
time.sleep(0.3)
pyautogui.press('alt')
pyautogui.press('n')
pyautogui.press(['down', 'down', 'down'])
pyautogui.press('enter')

# Step 5: New actions
time.sleep(6)

# Sequence:
pyautogui.hotkey('alt', 'tab')
pyautogui.press('space')

# 1st window
pyautogui.hotkey('alt', 'tab')
pyautogui.press(['down'] * 5)
pyautogui.press('space')

# 2nd window
pyautogui.hotkey('alt', 'tab')
pyautogui.press('down')
pyautogui.press('space')

# 1st window again
pyautogui.hotkey('alt', 'tab')
pyautogui.press(['down'] * 2)
pyautogui.press('space')

# 2nd window again
pyautogui.hotkey('alt', 'tab')
pyautogui.press(['down'] * 3)
pyautogui.press('space')

# 1st window again
pyautogui.hotkey('alt', 'tab')
pyautogui.press(['down'] * 4)
pyautogui.press('space')

print("Two TicTacToe windows opened, arranged, commands executed, and final sequence done.")
