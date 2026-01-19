import time
import struct
import os
import numpy as np

dir = r"C:\Users\User\AppData\Roaming\Ryujinx\sdcard"
#dir = r"C:\Users\User\AppData\Roaming\yuzu\sdmc"
rng = np.random.default_rng()

rows = [0, 0, 0, 0, 14, 0, 14, 0]
rwidth = 21
def genstate():
	l = []
	for rlen in rows:
		r = np.zeros(rwidth, dtype=np.bool)
		for i in range(rlen):
			r[i] = True
		rng.shuffle(r)
		l.append(r)
	return np.concatenate(l,axis=None)

def indtocoord(i):
	return (i % 21, i // 21)

def statetopixel(x, y):
	return ((3 + (y % 2) + x)*16+8, (50 - 2*y)*16+8)

def readexact(f, s):
	a = bytearray(s)
	v = memoryview(a)
	p = 0
	while p < s:
		p += f.readinto(v[p:])
		if p < s:
			time.sleep(1)
	return a

def readresult():
	return int(readexact(f_output, 5).decode("ascii"))

def saferename(old, new):
	while True:
		try:
			os.rename(old, new)
			break
		except OSError:
			time.sleep(1)

tas =  (
"o   002900\n"
"d   000000 -\n"
"u   000150 -\n"
"d   000150 r\n"
"u   000330 r\n"
"d   002900 -\n"
"u   002920 -\n"
"u   003080 -\n"
)

def writestate(s):
	with open(dir+r"\command_new.txt","w") as f:
		f.write(tas)
		for i, val in np.ndenumerate(s):
			attr = 0x06000240 if val else 0x06000040 # delete block if 1, keep if 0
			(x,y) = statetopixel(*indtocoord(i[0]))
			f.write("map 1 ")
			f.write(struct.pack('>2f2I', float(x), float(y), 0xFFFF0004, attr).hex(' ', 4))
			f.write("\n")
	saferename(dir+r"\command_new.txt", dir+r"\command.txt")

def logresult(s, v):
	f_log.write(str(v))
	for x in np.nditer(s):
		f_log.write(",1" if x else ",0")
	f_log.write("\n")
	f_log.flush()

def evalstate(s):
	writestate(s)
	return readresult()

def optimize():
	bestval = 0
	while True:
		x = genstate()
		v = evalstate(x)
		logresult(x, v)
		if v == bestval:
			print(f"Equal best val {v}:\n" + str(x))
		elif v > bestval:
			bestval = v
			print(f"New best val {v}:\n" + str(x))
		else:
			print(f"Val {v}")

time.sleep(3)
with open(dir+r"\out.log", "rb", buffering=0) as f_output, open("results_pow14_koop14.log", "a") as f_log:
	optimize()