#!/usr/bin/python
import PIL.Image
import struct


def bitflip(v):
    v = ((v & 0xffff0000) >> 16) | ((v & 0x0000ffff) << 16)
    v = ((v & 0xff00ff00) >> 8) | ((v & 0x00ff00ff) << 8)
    v = ((v & 0xf0f0f0f0) >> 4) | ((v & 0x0f0f0f0f) << 4)
    v = ((v & 0xcccccccc) >> 2) | ((v & 0x33333333) << 2)
    return ((v & 0xaaaaaaaa) >> 1) | ((v & 0x55555555) << 1)


def image_to_ledpanel_bytes(img: PIL.Image) -> bytes:

    if img.mode != '1':
        img = img.convert('1')
    bitdata = img.tobytes()

    ret = bytearray()

    # number of bytes, number of u32 words, zeros to pad
    bytes_row = (img.size[0] + 7) // 8
    u32_row = (bytes_row + 3) // 4
    zero_pad = bytes([0 for i in range(u32_row*4 - bytes_row)])

    for offs in range(0, len(bitdata), bytes_row):
        # 32bit words, but with wront byteorder, MSB of first byte
        # is first pixel!
        rowdata = bitdata[offs:offs+bytes_row] + zero_pad
        u32words = [bitflip(v) for v in struct.unpack(f'>{u32_row}I', rowdata)]
        ret += struct.pack(f'<{u32_row}I', *u32words)

    return bytes(ret)


if __name__ == '__main__':
    import argparse
    from pathlib import Path

    parser = argparse.ArgumentParser()
    parser.add_argument('pngfile', type=Path)
    args = parser.parse_args()

    img = PIL.Image.open(args.pngfile)
    data = image_to_ledpanel_bytes(img)
    print(data)
