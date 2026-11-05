#!/usr/bin/env python3
# Fast circles under each sending setup, scored only by direct screenshots of start and end.
import sys,time,os,statistics as st,math,itertools,threading,subprocess,re
sp=sys.argv[1];sys.path.insert(0,sp);import ready as RD
c=RD.c;PX=2.883;n=itertools.count();PINGS=[]
def pinger():
    p=subprocess.Popen(["ping","-i","0.2","192.168.50.182"],stdout=subprocess.PIPE,text=True)
    for l in p.stdout:
        m=re.search(r"time=([\d.]+)",l);PINGS.append((time.time(),float(m[1]) if m else 999.0))
threading.Thread(target=pinger,daemon=True).start()
def shot():
    p=f"{sp}/fs_{os.getpid()}_{next(n)%6}.pgm"
    if os.path.exists(p): open(p,"wb").close()
    c("shot "+p)
    for _ in range(100):
        time.sleep(.03)
        if os.path.exists(p) and os.path.getsize(p)>1000: time.sleep(.1);break
    d=open(p,"rb").read();_,dim,_,raw=d.split(b"\n",3);w,h=map(int,dim.split());return raw,w,h
def direct(a,b):
    ra,w,h=a;rb,_,_=b;f=max(1,w//160);xs=range(int(w*.28),int(w*.72),f*2);ys=range(int(h*.09),int(h*.5),f*2)
    def sad(dx,dy,s=1):
        t=0;k=0
        for y in ys[::s]:
            ny=y+dy
            if ny<0 or ny>=h: continue
            for x in xs[::s]:
                nx=x+dx
                if 0<=nx<w: t+=abs(ra[y*w+x]-rb[ny*w+nx]);k+=1
        return t/max(k,1)
    best=None;bs=(0,0)
    for dy in range(-30*f,31*f,f):
        for dx in range(-44*f,45*f,f):
            v=sad(dx,dy,2)
            if best is None or v<best: best=v;bs=(dx,dy)
    cx,cy=bs;best=None
    for dy in range(cy-f,cy+f+1):
        for dx in range(cx-f,cx+f+1):
            v=sad(dx,dy)
            if best is None or v<best: best=v;bs=(dx,dy)
    return -bs[0]*320/w/PX,bs[1]*320/w/PX
c("capture 1")
SET=[("as is (resend 200, 250 Hz)",200,4),("resend 16 ms",16,4),("cap 125 Hz",200,8),("resend 16 + cap 125 Hz",16,8),("as is again",200,4)]
CASES=[(10,300,8,"r10 209 deg/s"),(5,250,12,"r5 126 deg/s"),(20,500,6,"r20 251 deg/s")]
print(f"{'setup':28} {'circle':16} {'ended off, each run (deg)':42} {'mean':>5} {'worst ping':>10}",flush=True)
res={}
for name,rs,tk in SET:
    c(f"set resend {rs}");c(f"set tick {tk}");tot=[]
    for R,T,N,cn in CASES:
        ends=[];t0=time.time()
        for r in range(4):
            RD.ready(sp,log=False);a=shot();c(f"circle {R} {T} {N}");time.sleep(.9);b=shot();x,y=direct(a,b);ends.append(math.hypot(x,y))
        tot+=ends;pm=max([v for t,v in PINGS if t>=t0] or [0])
        print(f"{name:28} {cn:16} {'  '.join(f'{e:5.2f}' for e in ends):42} {st.mean(ends):5.2f} {pm:8.0f}ms",flush=True)
    print(f"{name:28} ALL mean {st.mean(tot):.2f}, median {st.median(tot):.2f}, worst {max(tot):.2f}\n",flush=True)
c("set resend 200");c("set tick 4");c("capture 0")
