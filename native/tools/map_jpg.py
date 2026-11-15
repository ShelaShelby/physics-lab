# 0.26: part of making assets/maps (see assets/maps/README.md).
# Resize a map to W x H (Lanczos) and save it as a baseline JPEG for the app (quality 90).
import sys
from PIL import Image
Image.MAX_IMAGE_PIXELS = None
src, out, W, H = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
im = Image.open(src).convert('RGB')
if im.size != (W, H): im = im.resize((W, H), Image.LANCZOS)
im.save(out, 'JPEG', quality=90, optimize=True)
print(out, im.size)
