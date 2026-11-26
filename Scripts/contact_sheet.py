"""WaxAttackCapture 결과 폴더를 몽타주별 한 줄짜리 콘택트시트로 합친다.

사용: python3 Scripts/contact_sheet.py <capture_dir> [out.png]
HIT 프레임은 빨간 테두리로 강조한다.
"""
import sys
from collections import OrderedDict
from pathlib import Path

from PIL import Image, ImageDraw

THUMB_W = 400


def main():
    src = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else src / 'contact_sheet.png'
    rows = OrderedDict()
    for p in sorted(src.glob('*.png'), key=lambda x: int(x.stem.split('_t')[0].rsplit('_', 1)[1])):
        if p.name == out.name:
            continue
        montage = p.stem.split('_t')[0].rsplit('_', 1)[0]
        rows.setdefault(montage, []).append(p)

    thumbs = {}
    for montage, files in rows.items():
        for f in files:
            im = Image.open(f).convert('RGB')
            h = int(im.height * THUMB_W / im.width)
            thumbs[f] = im.resize((THUMB_W, h))
    th = next(iter(thumbs.values())).height
    label_h = 22
    cols = max(len(v) for v in rows.values())
    sheet = Image.new('RGB', (cols * THUMB_W, len(rows) * (th + label_h)), (20, 20, 20))
    draw = ImageDraw.Draw(sheet)
    for r, (montage, files) in enumerate(rows.items()):
        y = r * (th + label_h)
        for c, f in enumerate(files):
            x = c * THUMB_W
            sheet.paste(thumbs[f], (x, y + label_h))
            tag = f.stem.split('_t', 1)[1]
            is_hit = tag.endswith('HIT')
            draw.text((x + 6, y + 4), f'{montage}  t{tag}', fill=(255, 80, 80) if is_hit else (220, 220, 220))
            if is_hit:
                draw.rectangle([x, y + label_h, x + THUMB_W - 1, y + label_h + th - 1], outline=(255, 0, 0), width=4)
    sheet.save(out)
    print(out)


if __name__ == '__main__':
    main()
