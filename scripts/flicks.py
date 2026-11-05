#!/usr/bin/env python3
# Real flicks at each queue size, scored only by direct screenshots.
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
print(f"{'queue':>6} {'move':24} {'want':>6} {'got, each run':34} {'err':>6} {'worst ping':>10}",flush=True)
MOVES=[("flick 5deg 60ms",[(5,60,0)],5),("flick 10deg 80ms",[(10,80,0)],10),("flick 20deg 100ms",[(20,100,0)],20),("out+back 45deg 120ms",[(45,120,300),(-45,120,0)],0),("out+back 90deg 150ms",[(90,150,300),(-90,150,0)],0),("out+back 90deg 300ms",[(90,300,300),(-90,300,0)],0),("snap 45 back, no pause",[(45,100,0),(-45,100,0)],0)]
summary={}
for q in(24,120,300):
    c(f"set overflow {q}");allerr=[]
    for name,seq,want in MOVES:
        got=[];t0=time.time()
        for r in range(3):
            sg=1 if r%2==0 else -1
            RD.ready(sp,log=False);a=shot()
            for d,ms,gap in seq:
                c(f"flick {sg*d} {ms}")
                if gap: time.sleep(gap/1000)
            time.sleep(1.0);b=shot();x,y=direct(a,b);got.append(x*sg);allerr.append(math.hypot(x*sg-want,y))
        pm=max([v for t,v in PINGS if t>=t0] or [0])
        print(f"{q:6d} {name:24} {want:6.1f} {'  '.join(f'{g:6.2f}' for g in got):34} {st.mean(got)-want:+6.2f} {pm:8.0f}ms",flush=True)
    summary[q]=allerr;print(f"{q:6d} ALL mean error {st.mean(allerr):.2f}, median {st.median(allerr):.2f}, worst {max(allerr):.2f}\n",flush=True)
c("set overflow 24");c("capture 0")
