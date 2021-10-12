#!/usr/bin/python
import sys
import usb.core
import argparse

# from include/usb_if.h
USB_IF_REQUEST_RESET_WRITEPTR=0x0000
USB_IF_REQUEST_PANEL_ONOFF=0x0001
USB_IF_REQUEST_PANEL_BRIGHTNESS=0x0002

from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--reset', action='store_true')
parser.add_argument('--stop', action='store_true')
parser.add_argument('--start', action='store_true')
parser.add_argument('--bright', type=int)

args = parser.parse_args()

dev = usb.core.find(idVendor=0x4e65, idProduct=0x7264)
if dev is None :
    print('Could not find usb device!')
    sys.exit(1)

dev.set_configuration()

if args.reset :
    dev.ctrl_transfer(0x40, USB_IF_REQUEST_RESET_WRITEPTR)
elif args.stop :
    dev.ctrl_transfer(0x40, USB_IF_REQUEST_PANEL_ONOFF, 0)
elif args.start :
    dev.ctrl_transfer(0x40, USB_IF_REQUEST_PANEL_ONOFF, 1)
elif args.bright is not None :
    dev.ctrl_transfer(0x40, USB_IF_REQUEST_PANEL_BRIGHTNESS, args.bright)


