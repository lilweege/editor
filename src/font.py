#!/usr/bin/env python3
import sys
import pathlib
import os
from PIL import Image, ImageFont, ImageDraw # pip install pillow

if len(sys.argv) != 3:
	print(f"Usage: {sys.argv[0]} <path/to/font.ttf> <size>")
	exit(1)

# check ttf file
filename = sys.argv[1]
if not pathlib.Path(filename).is_file():
	print(f"File not found: '{filename}' does not exist")
	exit(1)

if not filename.endswith(".ttf") and not filename.endswith(".otf"):
	print(f"Incorrect filetype: '{filename}' must end with .ttf")
	exit(1)

# check size
try:
	size = int(sys.argv[2])
	if size <= 0: raise ValueError()
except ValueError:
	print(f"Could not interpret '{sys.argv[2]}' as a positive integer")
	exit(1)

font = ImageFont.truetype(filename, size)
chars = ''.join(map(chr, range(ord(' '),ord('~')+1)))

# take the largest character width (monospace)
max_width = max(font.getmask(char).size[0] for char in chars)

WIDTH = len(chars) * max_width
HEIGHT = size * 2 # bigger, cropped after
im = Image.new("RGBA", (WIDTH, HEIGHT))
draw = ImageDraw.Draw(im)
for idx, char in enumerate(chars):
	# draw characters individually
	draw.text((idx*max_width+0.5*max_width, 0), char, font=font, anchor="ma")

# remove extra space
*_, height = im.getbbox()
im = im.crop((0, 0, im.width, height))


basename = os.path.basename(filename)[:-4]
im.save(f"{basename}.png")

# # write to stdout in c-style
# print(f"const int {basename}CharWidth = {max_width};")
# print(f"const int {basename}CharHeight = {im.height};")
# print(f"const int {basename}BitmapWidth = {im.width};")
# print(f"const int {basename}BitmapHeight = {im.height};")
# print(f"const unsigned char {basename}Bitmap[{im.width*im.height}] = {{")
# skip = len(im.getbands())
# res = ", ".join(map(lambda x: "{:#04x}".format(x), im.tobytes()[::skip]))
# res = "\t"+"\n\t".join(res[i:i+6*im.height] for i in range(0, len(res), 6*im.height))
# print(f"{res}\n}};")

