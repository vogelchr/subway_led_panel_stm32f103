#!/usr/bin/python
import time
import usb.core
import PIL.Image
import argparse
import struct
import ledpanel_tools

from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('pngfile', type=Path, help='png file to read')

args = parser.parse_args()

dev = usb.core.find(idVendor=0x0483, idProduct=0x5740)
dev.set_configuration()

img = PIL.Image.open(args.pngfile)
output = ledpanel_tools.image_to_ledpanel_bytes(img)

# any control transfer will reset the write pointer ;-)
dev.ctrl_transfer(0x40, 0)
dev.write(0x01, output)
