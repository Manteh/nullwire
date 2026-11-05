#!/usr/bin/env python3
# Same movements with input fixes off and on, with and without a Wi-Fi scan running. Scored by direct screenshots.
import sys,time,os,statistics as st,math,itertools,threading,subprocess,re
s=open(sys.argv[1]+"/fast.py").read();exec(s[:s.index('c("capture 1")')])
c("capture 1")
OFF=dict(resend=200,extra=0,qos=0);ON=dict(resend=16,extra=1,qos=1)
def scan(): subprocess.run(["system_profiler","SPAirPortDataType"],stdout=subprocess.DEVNULL)
def circle(sg): c(f"circle {10} {300} {8}")
def outback(sg):
    c(f"flick {sg*90} 200");time.sleep(.25);c(f"flick {-sg*90} 200")
def stopgo(sg):
    for d in(20,-35,15,-25,25): c(f"flick {sg*d} 90");time.sleep(.09)
CASES=[("circle r10 209 deg/s",circle),("out+back 90deg",outback),("5 quick flicks, net 0",stopgo)]
res={}
print(f"{'fixes':5} {'wifi':6} {'move':24} {'ended off, each run':34} {'mean':>5} {'worst ping':>10}",flush=True)
for block in range(2):
  for name,cfg in(("off",OFF),("on",ON)):
    for k,v in cfg.items(): print("  ",k,c(f"set {k} {v}")) if k=="qos" and block==0 else c(f"set {k} {v}")
    for sc in(False,True):
      for cn,f in CASES:
        e=[];t0=time.time()
        for r in range(2):
            sg=1 if r%2==0 else -1
            RD.ready(sp,log=False);a=shot()
            th=None
            if sc:
                th=threading.Thread(target=scan);th.start();time.sleep(.6)
            f(sg);time.sleep(.9)
            if th: th.join();time.sleep(.5)
            b=shot();x,y=direct(a,b);e.append(math.hypot(x,y));res.setdefault((name,sc,cn),[]).append(e[-1])
        pm=max([v for t,v in PINGS if t>=t0] or [0])
        print(f"{name:5} {'scan' if sc else 'quiet':6} {cn:24} {'  '.join(f'{x:5.2f}' for x in e):34} {st.mean(e):5.2f} {pm:8.0f}ms",flush=True)
print("\nSUMMARY (mean end error, all runs)")
for sc in(False,True):
    for name in("off","on"):
        v=[x for (n,w,_),l in res.items() if n==name and w==sc for x in l]
        print(f"  wifi {'scan' if sc else 'quiet':6} fixes {name:3}: mean {st.mean(v):5.2f}  median {st.median(v):5.2f}  worst {max(v):5.2f}  runs {len(v)}")
for k,v in dict(resend=200,extra=0,qos=0).items(): c(f"set {k} {v}")
c("capture 0")
