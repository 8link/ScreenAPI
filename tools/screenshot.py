#!/usr/bin/env python3
"""Saves the device screen as a PNG (PROJECT.md F-012).

Sends 'S' over the serial port and converts the frame the firmware returns:
"indexed565" (palette line, then palette indices; since 0.0.15) or "rgb332"
(8-bit pixels; 0.0.11 to 0.0.14). The port is opened without resetting the board, so the current
screen is captured.

Usage: tools/screenshot.py [output.png] [--port /dev/ttyUSB0] [--scale 3]
"""
import argparse
import sys
import time

import serial
from PIL import Image


def rgb332_to_rgb888(value):
    r = (value >> 5) & 0x07
    g = (value >> 2) & 0x07
    b = value & 0x03
    return (r * 255 // 7, g * 255 // 7, b * 255 // 3)


def rgb565_to_rgb888(value):
    r = (value >> 11) & 0x1F
    g = (value >> 5) & 0x3F
    b = value & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def read_line(link):
    return link.readline().decode(errors="replace").strip()


def capture(port):
    # Opening raises DTR and RTS together, which the auto-reset circuit ignores.
    # Releasing RTS before DTR keeps EN high; the other order resets the board
    # (BOARDS.md, UART).
    link = serial.Serial(port, 115200, timeout=10)
    link.rts = False
    link.dtr = False
    try:
        time.sleep(0.2)
        link.reset_input_buffer()
        link.write(b"S")
        deadline = time.time() + 10
        while True:
            line = link.readline().decode(errors="replace").strip()
            if line.startswith("SCREENSHOT"):
                break
            if time.time() > deadline:
                sys.exit("no SCREENSHOT header from the device")
        parts = line.split()
        if len(parts) != 4 or parts[3] not in ("rgb332", "indexed565"):
            sys.exit(f"unexpected header: {line}")
        width, height, pixel_format = int(parts[1]), int(parts[2]), parts[3]
        if pixel_format == "indexed565":
            palette_hex = read_line(link)
            if len(palette_hex) != 256 * 4:
                sys.exit(f"palette line has {len(palette_hex)} hex digits, expected 1024")
            palette = [rgb565_to_rgb888(int(palette_hex[i:i + 4], 16)) for i in range(0, 1024, 4)]
        else:
            palette = [rgb332_to_rgb888(value) for value in range(256)]
        rows = []
        for _ in range(height):
            row = read_line(link)
            if len(row) != width * 2:
                sys.exit(f"row {len(rows)} has {len(row)} hex digits, expected {width * 2}")
            rows.append(bytes.fromhex(row))
        if read_line(link) != "END":
            sys.exit("missing END line")
    finally:
        link.close()
    return width, height, palette, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output", nargs="?", default="screenshot.png")
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--scale", type=int, default=3)
    args = parser.parse_args()

    width, height, palette, rows = capture(args.port)
    image = Image.new("RGB", (width, height))
    image.putdata([palette[value] for row in rows for value in row])
    if args.scale > 1:
        image = image.resize((width * args.scale, height * args.scale), Image.NEAREST)
    image.save(args.output)
    print(f"saved {args.output} ({width} x {height}, scale {args.scale})")


if __name__ == "__main__":
    main()
