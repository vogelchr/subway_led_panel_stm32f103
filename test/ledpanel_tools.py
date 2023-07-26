#!/usr/bin/python
import PIL.Image
import struct


def image_to_ledpanel_bytes(img: PIL.Image) -> bytes:

    if img.mode != '1':
        img = img.convert('1')
    return img.tobytes()

if __name__ == '__main__':
    import argparse
    from pathlib import Path

    parser = argparse.ArgumentParser()
    parser.add_argument('pngfile', type=Path)
    args = parser.parse_args()

    img = PIL.Image.open(args.pngfile)
    data = image_to_ledpanel_bytes(img)
    print(data)
