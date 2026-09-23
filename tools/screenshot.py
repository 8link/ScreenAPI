#!/usr/bin/env python3
"""Saves the device screen as a PNG (PROJECT.md F-012).

Sends 'S' over the serial port and converts the RGB332 frame the firmware
returns. The port is opened without resetting the board, so the current
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
        if len(parts) != 4 or parts[3] != "rgb332":
            sys.exit(f"unexpected header: {line}")
        width, height = int(parts[1]), int(parts[2])
        rows = []
        for _ in range(height):
            row = link.readline().decode(errors="replace").strip()
            if len(row) != width * 2:
                sys.exit(f"row {len(rows)} has {len(row)} hex digits, expected {width * 2}")
            rows.append(bytes.fromhex(row))
        if link.readline().decode(errors="replace").strip() != "END":
            sys.exit("missing END line")
    finally:
        link.close()
    return width, height, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output", nargs="?", default="screenshot.png")
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--scale", type=int, default=3)
    args = parser.parse_args()

    width, height, rows = capture(args.port)
    image = Image.new("RGB", (width, height))
    image.putdata([rgb332_to_rgb888(value) for row in rows for value in row])
    if args.scale > 1:
        image = image.resize((width * args.scale, height * args.scale), Image.NEAREST)
    image.save(args.output)
    print(f"saved {args.output} ({width} x {height}, scale {args.scale})")


if __name__ == "__main__":
    main()
