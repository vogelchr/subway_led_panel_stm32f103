#!/usr/bin/python
import sys
import time
import usb.core
import PIL.Image
import argparse
import struct
import ledpanel_tools

from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('pngfile', type=Path, help='png file to read')
parser.add_argument('-W', '--width', metavar='pixels',
                    type=int, default=120, help='width [def:%(default)d]')
parser.add_argument('-H', '--height', metavar='pixels',
                    type=int, default=20, help='height [def:%(default)d]')

args = parser.parse_args()

dev = usb.core.find(idVendor=0x4e65, idProduct=0x7264)
if dev is None :
    print('Could not find usb device!')
    sys.exit(1)

dev.set_configuration()

img = PIL.Image.open(args.pngfile)

if img.size[1] != args.height :
    print('Image height must match panel height!')
    sys.exit(1)

if img.size[0] < args.width :
    print('Image width must be larger or equal to panel width!')

# any control transfer will reset the write pointer ;-)
dev.ctrl_transfer(0x40, 0)

for dx in range(img.size[0] - args.width) :
    img_crop = img.crop((dx, 0, dx+args.width, args.height))
    output = ledpanel_tools.image_to_ledpanel_bytes(img_crop)
    print(img_crop.size)
    dev.write(0x01, output)
    time.sleep(0.025)
