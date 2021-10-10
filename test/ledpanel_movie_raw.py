#!/usr/bin/python
import time
import usb.core
import argparse
import struct
from pathlib import Path
import PIL.Image
import ledpanel_tools

parser = argparse.ArgumentParser()
parser.add_argument('-W', '--width', metavar='pixels',
                    type=int, default=40, help='width [def:%(default)d]')
parser.add_argument('-H', '--height', metavar='pixels',
                    type=int, default=20, help='height [def:%(default)d]')
parser.add_argument('raw_movie_file', type=Path,
                    help='raw movie file in gray width x height to read')
args = parser.parse_args()


dev = usb.core.find(idVendor=0x4e65, idProduct=0x7264)
dev.set_configuration()
dev.ctrl_transfer(0x40, 0) # any control transfer will reset the write pointer ;-)

rawmovie = args.raw_movie_file.open('rb')

frameno=0
while True:
    rawdata = rawmovie.read(args.width * args.height)
    if len(rawdata) < args.width * args.height :
        break

    img = PIL.Image.frombytes('L', (args.width, args.height), rawdata)
    output = ledpanel_tools.image_to_ledpanel_bytes(img)
    dev.write(0x01, output)

    print(frameno)
    frameno += 1
    time.sleep(0.05)