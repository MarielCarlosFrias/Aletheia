from PIL import Image, ImageDraw

size = 512
img = Image.new('RGBA', (size, size), (241, 245, 249, 255))
draw = ImageDraw.Draw(img)

# rounded square mask
radius = 96
# draw checkerboard background
bg_color1 = (226, 232, 240, 255)
bg_color2 = (248, 250, 252, 255)
cell = size // 16
for y in range(0, size, cell):
    for x in range(0, size, cell):
        if (x//cell + y//cell) % 2 == 0:
            draw.rectangle([x, y, x+cell, y+cell], fill=bg_color1)

# clip to rounded square
mask = Image.new('L', (size, size), 0)
mask_draw = ImageDraw.Draw(mask)
mask_draw.rounded_rectangle([0,0,size,size], radius=radius, fill=255)
img.putalpha(mask)

# draw silhouette
draw = ImageDraw.Draw(img)
# head
head_center = (size//2, size//3)
head_r = 76
draw.ellipse([head_center[0]-head_r, head_center[1]-head_r, head_center[0]+head_r, head_center[1]+head_r], fill=(14,165,233,255))
# body
body_box = [size//2 - 128, size//2, size//2 + 128, size-32]
draw.ellipse(body_box, fill=(14,165,233,255))

# dashed outline
# simple approximation with lines
# Use white stroke
outline_color = (255,255,255,230)
# head outline
draw.ellipse([head_center[0]-head_r, head_center[1]-head_r, head_center[0]+head_r, head_center[1]+head_r], outline=outline_color, width=6)

img.save('/home/theking1x/projetos/bg-remover/assets/aletheia.png')
print('saved')
