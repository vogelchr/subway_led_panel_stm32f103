#!/usr/bin/python
import time
import usb.core
import PIL.Image
import argparse
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('-W', '--width', metavar='pixels',
                    type=int, default=40, help='width [def:%(default)d]')
parser.add_argument('-H', '--height', metavar='pixels',
                    type=int, default=20, help='height [def:%(default)d]')
parser.add_argument('pngfile', type=Path, help='png file to read')

args = parser.parse_args()

img = PIL.Image.open(args.pngfile)
if img.mode != 'L':
    img = img.convert('L')

output = bytearray()

dev = usb.core.find(idVendor=0x0483, idProduct=0x5740)
dev.set_configuration()

# any control transfer will reset the write pointer ;-)
dev.ctrl_transfer(0x40, 0)

for y in range(args.height):
    for k in range((args.width + 31) // 32) :
        x = 32 * k
        word = 0
    
        for dx in range(min(img.size[0]-x, 32)) :
            if x+dx < img.size[0] and y < img.size[1]:
                if img.getpixel((x+dx, y)) > 0x7f:
                    word = word | 1<<dx

        output += struct.pack('I', word)
        if len(output) >= 64 :
            print(output)
            dev.write(0x01, output[0:64])
            del output[0:64]

if len(output) :
    dev.write(0x01, output)
    output.clear()
