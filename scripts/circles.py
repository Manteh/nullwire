#!/usr/bin/env python3
# Mouse draws exact circles; measures what circle the camera really drew, and whether it came back to the start.
import socket,time,sys,math,statistics as st,os,json,subprocess,threading,re
PINGS=[]
def pinger():
    p=subprocess.Popen(["ping","-i","0.2","192.168.50.182"],stdout=subprocess.PIPE,text=True)
    for l in p.stdout:
        m=re.search(r"time=([\d.]+)",l)
        PINGS.append((time.time(),float(m[1]) if m else 999.0))
threading.Thread(target=pinger,daemon=True).start()
sp=sys.argv[1];sys.path.insert(0,sp);import ready as RD;WMAX=float(sys.argv[2]);s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.settimeout(90);addr=("127.0.0.1",47999)
def c(m):
    s.sendto(m.encode(),addr);r=s.recv(300).decode()
    if r in("bad command","unknown"): sys.exit("REJECTED "+m)
    return r
def shot():
    p=f"{sp}/c.pgm"
    if os.path.exists(p): os.remove(p)
    c("shot "+p)
    for _ in range(100):
        time.sleep(.03)
        if os.path.exists(p) and os.path.getsize(p)>1000: time.sleep(.1);break
    d=open(p,"rb").read();_,dim,_,raw=d.split(b"\n",3);w,h=map(int,dim.split());return raw,w,h
def direct(a,b):
    # compare two screenshots directly: how far did the scene shift? (independent of the tracker)
    ra,w,h=a;rb,_,_=b;best=None;bs=(0,0);f=w//160
    xs=range(int(w*.28),int(w*.72),f*2);ys=range(int(h*.09),int(h*.5),f*2)
    def sad(dx,dy,st=1):
        t=0;n=0
        for y in ys[::st]:
            ny=y+dy
            if ny<0 or ny>=h: continue
            for x in xs[::st]:
                nx=x+dx
                if 0<=nx<w: t+=abs(ra[y*w+x]-rb[ny*w+nx]);n+=1
        return t/max(n,1)
    for dy in range(-40*f//4,41*f//4,f):
        for dx in range(-48*f//4,49*f//4,f):
            v=sad(dx,dy,2)
            if best is None or v<best: best=v;bs=(dx,dy)
    cx,cy=bs;best=None
    for dy in range(cy-f,cy+f+1):
        for dx in range(cx-f,cx+f+1):
            v=sad(dx,dy)
            if best is None or v<best: best=v;bs=(dx,dy)
    return bs[0]*320/w,bs[1]*320/w
def run(x,y,ms):
    c("t2d 1");time.sleep(.2);c(f"hold2 {x} {y} {ms}");time.sleep(.5);px,py,fr,bad=c("t2dget").split();c("t2d 0");return float(px),float(py)
def itp(t,ts,v):
    lo,hi=0,len(ts)-1
    while hi-lo>1:
        m=(lo+hi)//2
        if ts[m]<=t: lo=m
        else: hi=m
    u=(t-ts[lo])/max(ts[hi]-ts[lo],1e-6);return v[lo]+(v[hi]-v[lo])*min(max(u,0),1)
c("capture 1")
RD.ready(sp)
sc=[abs(run(g,0,1000*360/WMAX)[0])/360 for g in(1,-1,1,-1,1,-1)];PX=st.median(sc);RD.ready(sp);
if not 2.4<PX<3.4: sys.exit(f"BAD SCALE {PX:.3f}, spread {max(sc)-min(sc):.3f}: not measuring")
print(f"scale {PX:.3f} px/deg, spread {max(sc)-min(sc):.3f}",flush=True)
print(f"\n{'circle':24} {'speed':>7} | {'wide':>6} {'tall':>6} {'tall/wide':>9} {'roundness':>9} {'lag ms':>6} | {'path err':>8} {'back at start (tracker)':>24} {'(screenshots)':>14} {'worst ping':>10}",flush=True)
cases=[(3,2500,2,"tiny slow"),(5,3000,2,"small slow"),(5,800,4,"small quick"),(10,1500,3,"med medium"),(20,1200,3,"big medium"),(10,300,8,"med very fast"),(20,500,6,"big fast"),(30,600,5,"big over limit")]
out=[]
SETUPS=[("old",{"vratio":.735,"axispull":0,"diagloss":0}),("vertical fix",{"vratio":.68,"axispull":0,"diagloss":0}),("vertical + pull fix",{"vratio":.68,"axispull":10.7,"diagloss":.06})]
for tag,cfg in SETUPS:
 for k,v in cfg.items(): c(f"set {k} {v}")
 print(f"=== {tag}: {cfg}",flush=True)
 for R,T,N,name in cases:
  for rep in range(2):
    RD.ready(sp,log=False);tp0=time.time();a=shot();c("t2d 1");time.sleep(.25);c(f"circle {R} {T} {N}");time.sleep(.9);f=f"{sp}/path.txt";c("t2ddump "+f);got=c("t2dget");c("t2d 0");b=shot()
    A=[];B=[]
    for l in open(f):
        k,t,x,y=l.split();(A if k=="asked" else B).append((float(t),float(x),float(y)))
    os.remove(f);ts=[p[0] for p in B];sx=[-p[1]/PX for p in B];sy=[p[2]/PX for p in B];t0=A[0][0];x0=itp(t0,ts,sx);y0=itp(t0,ts,sy)
    best=None
    for lag in range(0,400,5):
        e=0
        for t,x,y in A[::6]: e+=(itp(t+lag/1000,ts,sx)-x0-x)**2+(itp(t+lag/1000,ts,sy)-y0-y)**2
        e=math.sqrt(e/len(A[::6]))
        if best is None or e<best[0]: best=(e,lag)
    err,lag=best
    # use the middle turns only, after the start-up
    seg=[(t,x,y) for t,x,y in A if t-t0>T/1000*.5]
    gx=[itp(t+lag/1000,ts,sx)-x0 for t,_,_ in seg];gy=[itp(t+lag/1000,ts,sy)-y0 for t,_,_ in seg]
    cx,cy=st.mean(gx),st.mean(gy);rad=[math.hypot(p-cx,q-cy) for p,q in zip(gx,gy)]
    wide=(sorted(gx)[int(len(gx)*.98)]-sorted(gx)[int(len(gx)*.02)])/2;tall=(sorted(gy)[int(len(gy)*.98)]-sorted(gy)[int(len(gy)*.02)])/2
    endT=A[-1][0];ex=sx[-1]-x0;ey=sy[-1]-y0;dx,dy=direct(a,b)
    print(f"{name+f' r{R}':24} {2*math.pi*R/T*1000:5.0f}/s | {wide:6.2f} {tall:6.2f} {tall/max(wide,.01):9.2f} {st.pstdev(rad)/max(st.mean(rad),.01)*100:8.1f}% {lag:6d} | {err:6.2f}d x{ex:+7.2f} y{ey:+7.2f}        x{-dx/PX:+6.2f} y{dy/PX:+6.2f} {max([v for t,v in PINGS if t>=tp0] or [0]):8.0f}ms",flush=True)
    out.append(dict(tag=tag,end=math.hypot(dx,dy)/PX,pingmax=max([v for t,v in PINGS if t>=tp0] or [0]),name=name,R=R,T=T,rep=rep,lag=lag,wide=wide,tall=tall,err=err,sx=[p-x0 for p in gx][::3],sy=[q-y0 for q in gy][::3]))
json.dump(out,open(f"{sp}/circles.json","w"));c("capture 0")
