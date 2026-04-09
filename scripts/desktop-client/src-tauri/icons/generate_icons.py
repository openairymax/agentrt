#!/usr/bin/env python3
"""
AgentOS Desktop Client Icon Generator
Generates placeholder icons for all platforms

Requirements:
  pip install Pillow

Usage:
  python generate_icons.py
"""

import os
from PIL import Image, ImageDraw, ImageFont

ICON_DIR = os.path.dirname(os.path.abspath(__file__))

def create_icon(size: int, filename: str):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    margin = size // 8
    radius = size // 4
    
    draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=radius,
        fill=(26, 115, 232, 255)
    )
    
    try:
        font_size = size // 2
        font = ImageFont.truetype("arial.ttf", font_size)
    except:
        font = ImageFont.load_default()
    
    text = "A"
    bbox = draw.textbbox((0, 0), text, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    
    x = (size - text_width) // 2
    y = (size - text_height) // 2 - bbox[1]
    
    draw.text((x, y), text, fill=(255, 255, 255, 255), font=font)
    
    img.save(os.path.join(ICON_DIR, filename))
    print(f"Generated: {filename} ({size}x{size})")

def create_ico():
    sizes = [16, 32, 48, 64, 128, 256]
    images = []
    
    for size in sizes:
        img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        
        margin = size // 8
        radius = size // 4
        
        draw.rounded_rectangle(
            [margin, margin, size - margin, size - margin],
            radius=radius,
            fill=(26, 115, 232, 255)
        )
        
        try:
            font = ImageFont.truetype("arial.ttf", size // 2)
        except:
            font = ImageFont.load_default()
        
        text = "A"
        bbox = draw.textbbox((0, 0), text, font=font)
        text_width = bbox[2] - bbox[0]
        text_height = bbox[3] - bbox[1]
        
        x = (size - text_width) // 2
        y = (size - text_height) // 2 - bbox[1]
        
        draw.text((x, y), text, fill=(255, 255, 255, 255), font=font)
        images.append(img)
    
    ico_path = os.path.join(ICON_DIR, 'icon.ico')
    images[0].save(
        ico_path,
        format='ICO',
        sizes=[(img.width, img.height) for img in images],
        append_images=images[1:]
    )
    print(f"Generated: icon.ico (multi-size)")

def create_icns():
    sizes = [16, 32, 64, 128, 256, 512, 1024]
    
    for size in sizes:
        img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(img)
        
        margin = size // 8
        radius = size // 4
        
        draw.rounded_rectangle(
            [margin, margin, size - margin, size - margin],
            radius=radius,
            fill=(26, 115, 232, 255)
        )
        
        try:
            font = ImageFont.truetype("arial.ttf", size // 2)
        except:
            font = ImageFont.load_default()
        
        text = "A"
        bbox = draw.textbbox((0, 0), text, font=font)
        text_width = bbox[2] - bbox[0]
        text_height = bbox[3] - bbox[1]
        
        x = (size - text_width) // 2
        y = (size - text_height) // 2 - bbox[1]
        
        draw.text((x, y), text, fill=(255, 255, 255, 255), font=font)
        
        filename = f'icon_{size}x{size}.png'
        img.save(os.path.join(ICON_DIR, filename))
        print(f"Generated: {filename} ({size}x{size})")
    
    print("\nTo create .icns file on macOS:")
    print("1. Use 'iconutil' command or")
    print("2. Use online converter: https://cloudconvert.com/png-to-icns")

def main():
    print("=" * 60)
    print("AgentOS Desktop Client Icon Generator")
    print("=" * 60)
    print()
    
    create_icon(32, "32x32.png")
    create_icon(128, "128x128.png")
    create_icon(256, "128x128@2x.png")
    
    print("\nGenerating Windows ICO...")
    create_ico()
    
    print("\nGenerating macOS ICNS resources...")
    create_icns()
    
    print("\n" + "=" * 60)
    print("Icon generation complete!")
    print("=" * 60)
    print("\nGenerated files:")
    print("  - 32x32.png")
    print("  - 128x128.png")
    print("  - 128x128@2x.png")
    print("  - icon.ico (Windows)")
    print("  - icon_*x*.png (macOS)")
    print("\nNext steps:")
    print("  1. Replace placeholder icons with professional design")
    print("  2. On macOS, run: iconutil -c icns icons/ -o icon.icns")
    print("  3. Rebuild the application")

if __name__ == "__main__":
    try:
        from PIL import Image, ImageDraw, ImageFont
        main()
    except ImportError:
        print("Error: Pillow library not found")
        print("Install with: pip install Pillow")
