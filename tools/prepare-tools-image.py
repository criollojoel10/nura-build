#!/usr/bin/env python3
"""Inyecta kupfer-repartition en la imagen 'barebone+tools' de Kupfer.

Uso: prepare-tools-image.py IN_FULL.img OUT_FULL.img SCRIPT SERVICE MARK
- localiza la particion p2 (ext4) probando sectores de 512 y 4096,
- la extrae, escribe /usr/libexec/kupfer-repartition, la unit y el flag
  /etc/kupfer/repartition-once con debugfs, y la vuelve a insertar.
"""
import os, re, shutil, struct, subprocess, sys, tempfile

src, dst, script, service, mark, gptedit = sys.argv[1:7]
img = open(src, 'rb')
hdr = img.read(512)
assert hdr[510:512] == b'\x55\xaa', 'sin MBR'
parts = []
for i in range(4):
    e = hdr[446 + i * 16:446 + (i + 1) * 16]
    if e[4] == 0:
        continue
    parts.append((e[4], struct.unpack('<I', e[8:12])[0],
                  struct.unpack('<I', e[12:16])[0]))
print('MBR:', parts)

def is_ext4(off):
    img.seek(off + 1024 + 56)
    return img.read(2) == b'\x53\xef'

# identifica el tamano de sector y la particion p2 (la segunda)
for ss in (512, 4096):
    cand = parts
    if len(cand) < 2:
        continue
    p2 = cand[1]
    off = p2[1] * ss
    if is_ext4(off):
        sector_size, p2_off, p2_len = ss, off, p2[2] * ss
        break
else:
    sys.exit('no encuentro ext4 en p2 (¿layout nuevo?)')

print(f'sector={sector_size} p2_off={p2_off} p2_len={p2_len}')
tmp = tempfile.mkdtemp(prefix='toolsimg.')
p2 = os.path.join(tmp, 'p2.img')
img.seek(p2_off)
with open(p2, 'wb') as f:
    left = p2_len
    while left:
        b = img.read(min(left, 8 << 20))
        if not b:
            break
        f.write(b)
        left -= len(b)
img.close()

cmds = os.path.join(tmp, 'cmds')
with open(cmds, 'w') as f:
    f.write("""mkdir /etc/kupfer
mkdir /usr/libexec
mkdir /usr/lib/systemd/system
mkdir /etc/systemd/system/multi-user.target.wants
""")
    if not os.path.exists(script):
        sys.exit('falta ' + script)
    f.write(f'write {script} /usr/libexec/kupfer-repartition\n')
    f.write('sif /usr/libexec/kupfer-repartition mode 0100755\n')
    f.write(f'write {service} /usr/lib/systemd/system/kupfer-repartition.service\n')
    f.write('sif /usr/lib/systemd/system/kupfer-repartition.service mode 0100644\n')
    empty = os.path.join(tmp, 'empty')
    open(empty, 'w').close()
    # debugfs: symlink <nuevo> <destino>
    wants = '/etc/systemd/system/multi-user.target.wants/kupfer-repartition.service'
    f.write(f'rm {wants}\n')
    f.write(f'symlink {wants} /usr/lib/systemd/system/kupfer-repartition.service\n')
    f.write(f'write {empty} /etc/kupfer/repartition-once\n')
    f.write(f'write {gptedit} /usr/libexec/gpt-edit.py\n')
    f.write('sif /usr/libexec/gpt-edit.py mode 0100755\n')

r = subprocess.run(['debugfs', '-w', '-f', cmds, p2], capture_output=True, text=True)
if r.returncode:
    sys.exit('debugfs: ' + r.stderr)

# verifica
r = subprocess.run(['debugfs', '-R', 'cat /usr/libexec/kupfer-repartition', p2],
                   capture_output=True, text=True)
assert 'kupfer-repartition' in r.stdout, 'no se escribio el script'
r = subprocess.run(['debugfs', '-R', 'stat /etc/kupfer/repartition-once', p2],
                   capture_output=True, text=True)
assert 'Inode:' in r.stdout, 'no se creo el flag'
r = subprocess.run(['debugfs', '-R', 'stat /usr/libexec/gpt-edit.py', p2],
                   capture_output=True, text=True)
assert 'Type: regular' in r.stdout, 'no se copio gpt-edit.py'
r = subprocess.run(['debugfs', '-R', 'stat /usr/lib/systemd/system/kupfer-repartition.service', p2],
                   capture_output=True, text=True)
assert 'Type: regular' in r.stdout, 'la unit no es fichero regular'
r = subprocess.run(['debugfs', '-R', 'stat /etc/systemd/system/multi-user.target.wants/kupfer-repartition.service', p2],
                   capture_output=True, text=True)
assert 'Fast link dest' in r.stdout or 'Type: symlink' in r.stdout, 'no hay symlink de enable'
print('unit + symlink OK')

shutil.copyfile(src, dst)
with open(dst, 'r+b') as out:
    out.seek(p2_off)
    with open(p2, 'rb') as f:
        shutil.copyfileobj(f, out, 8 << 20)
print('OK', dst, os.path.getsize(dst))
