import time
import pyautogui
import struct
import numpy as np

#dir = r"C:\Users\User\AppData\Roaming\Ryujinx\sdcard"
dir = r"C:\Users\User\AppData\Roaming\yuzu\sdmc"
rng = np.random.default_rng()
def genstate():
	return rng.integers(0,2, size=(21,8), dtype=np.bool)

def statetopixel(x, y):
	return ((3 + (y % 2) + x)*16+8, (50 - 2*y)*16+8)

def neighbor(s):
	dim = s.shape
	x = rng.integers(dim[0])
	y = rng.integers(dim[1])
	s2 = s.copy()
	s2[x,y] = not s2[x,y]
	return s2

def readresult():
	with open(dir+r"\cc.txt","r") as f:
		return int(f.read(4))

def writestate(s):
	with open(dir+r"\blockmap.txt","w") as f:
		for i, val in np.ndenumerate(s):
			attr = 0x06000240 if val else 0x06000040
			mapped = statetopixel(i[0], i[1])
			f.write("1 ")
			f.write(struct.pack('>2f2I', float(mapped[0]), float(mapped[1]), 0xFFFF0004, attr).hex(' ', 4))
			f.write("\n")

def evalstate(s):
	writestate(s)

	pyautogui.keyDown('=') # Load new mapping
	time.sleep(0.1)
	pyautogui.keyDown('right')
	time.sleep(0.1)
	pyautogui.keyUp('right')
	pyautogui.keyUp('=')
	time.sleep(0.1)
	pyautogui.keyDown('=')
	time.sleep(0.1)
	pyautogui.keyUp('=') 
	time.sleep(0.1)

	pyautogui.keyDown('-') # Start
	time.sleep(1)
	pyautogui.keyUp('-')
	pyautogui.keyDown('right')
	time.sleep(3)
	pyautogui.keyUp('right')

	time.sleep(21)
	
	pyautogui.keyDown('=') # Save clear count
	time.sleep(0.1)
	pyautogui.keyDown('down')
	time.sleep(0.1)
	pyautogui.keyUp('down')
	pyautogui.keyUp('=')
	time.sleep(0.1)
	pyautogui.keyDown('=')
	time.sleep(0.1)
	pyautogui.keyUp('=')
	time.sleep(0.1)

	pyautogui.keyDown('-')
	time.sleep(0.1)
	pyautogui.keyUp('-')
	time.sleep(0.1)
	
	return readresult()

def optimize():
	bestval = 0
	while True:
		x = genstate()
		v = evalstate(x)
		if v == bestval:
			print(f"Equal best val {v}:\n" + str(x))
		elif v > bestval:
			bestval = v
			print(f"New best val {v}:\n" + str(x))
		else:
			print(f"Val {v}")

time.sleep(3)
optimize()