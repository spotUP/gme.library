"""Reference decoder for src/ezpk.c (check-ezpk compares the C output against this one's sha256).
Our own derivation of the format, kept with the notes in thoughts/spot/gymx-packed.md; no third-party code is copied.
Standalone EZPK (YMAMP 2.0b / SUPPORT_YMAMP20B_EZPK) GYMX unpacker.
Algorithm per PSP-Archive/MasterBoy psp/gym/gym.cpp load_ezpk_gym + bzlib with HEADER_COFS=5, BZ_HDR_{B,Z,h}=0x20.
Usage: python3 -I unezpk.py in.gym out.gym   (out = GYMX header with field@424 zeroed + plain GYM stream)"""
import sys,bz2,zlib,struct
u32=lambda b,o: struct.unpack_from('<I',b,o)[0]
def ez_bz2(s):
    s=bytearray(s)
    assert s[:3]==b'\x20\x20\x20'
    s[0:3]=b'BZh'
    for i in range(4,10): s[i]-=5          # block magic 31 41 59 26 53 59 stored +5
    pat=0x1c774a3d5595; orig=0x177245385090 # end magic stored +5 per byte, bit aligned
    bits=int.from_bytes(s,'big'); L=len(s)*8; m=(1<<48)-1; hit=None
    for pos in range(40,L-47):
        if (bits>>(L-pos-48))&m==pat: hit=pos
    assert hit is not None
    sh=L-hit-48
    bits=(bits&~(m<<sh))|(orig<<sh)
    return bz2.decompress(bits.to_bytes(len(s),'big'))
def decomp_dac(src):
    assert src[:4]==b'EZPK'
    ul=u32(src,4)
    return ez_bz2(src[8:]) if ul&0xf0000000==0 else zlib.decompressobj().decompress(src[8:])
def unpack(f):
    assert f[:4]==b'GYMX'
    total=u32(f,424); d=f[428:]
    assert d[:4]==b'EZPK'
    b=ez_bz2(d[8:])
    dacl=u32(b,0x18)+u32(b,0x1c)
    b=bytearray(b)
    if dacl>0:
        o=u32(b,0x20); r=decomp_dac(bytes(b[o:o+u32(b,0x14)])); b[o:o+len(r)]=r
    p=0x24; sec=[]
    for off in (0x00,0x04,0x08,0x0c,0x10,0x18):
        sec.append(p); p+=u32(b,off)
    typ,reg,datm,dats,sng,dacm=sec; dacs=p
    cur=dict(typ=typ,reg=reg,datm=datm,dats=dats,sng=sng,dacm=dacm,dacs=dacs)
    def nx(k): v=b[cur[k]]; cur[k]+=1; return v
    out=bytearray()
    while len(out)<total:
        t=nx('typ'); out.append(t)
        if t in(1,2):
            r=nx('reg'); out.append(r)
            if r==0x2a and dacl>0: out.append(nx('dacm' if t==1 else 'dacs'))
            else: out.append(nx('datm' if t==1 else 'dats'))
        elif t==3: out.append(nx('sng'))
    return f[:424]+b'\0\0\0\0'+bytes(out)
if __name__=='__main__':
    o=unpack(open(sys.argv[1],'rb').read()); open(sys.argv[2],'wb').write(o)
    print(len(o)-428, o[428:444].hex())
