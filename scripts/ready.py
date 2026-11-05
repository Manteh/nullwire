# Puts the camera somewhere measurable: level, and facing a detailed part of the room.
import socket,time,os,statistics as st,sys,itertools
_s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);_s.settimeout(20);_a=("127.0.0.1",47999);_n=itertools.count()
def c(m):
    _s.sendto(m.encode(),_a);r=_s.recv(300).decode()
    if r in("bad command","unknown"): sys.exit("REJECTED "+m)
    return r
def contrast(sp):
    p=f"{sp}/rd_{os.getpid()}_{next(_n)%8}.pgm"
    if os.path.exists(p): open(p,"wb").close()
    c("shot "+p)
    for _ in range(100):
        time.sleep(.03)
        if os.path.exists(p) and os.path.getsize(p)>1000: time.sleep(.1);break
    d=open(p,"rb").read();_,dim,_,raw=d.split(b"\n",3);w,h=map(int,dim.split())
    v=[raw[y*w+x] for y in range(int(h*.09),int(h*.78),3) for x in range(int(w*.28),int(w*.72),3) if not(x>w/2 and y>h/2)]
    return st.pstdev(v)
def level():
    c("hold2 0 1 1100");time.sleep(.15)       # up against the look-up limit, a known angle
    c("hold2 0 -0.5 790");time.sleep(.35)     # back down about 89 degrees
def ready(sp,need=18,log=True):
    level();b=contrast(sp)
    if b<need:
        for i in range(8):
            c("hold 0.5 280");time.sleep(.35);b=contrast(sp)
            if b>=need: break
    if log: print(f"[view contrast {b:.1f}]",flush=True)
    return b
