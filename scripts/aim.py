#!/usr/bin/env python3
# Sends exact mouse movements through the bridge and measures how far the real camera turned.
import socket,time,sys,statistics as st,json
PX_PER_DEG=float(sys.argv[1]) if len(sys.argv)>1 else 1338.9/360
SENS=float(sys.argv[2]) if len(sys.argv)>2 else 0.06
REPS=int(sys.argv[3]) if len(sys.argv)>3 else 3
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.settimeout(3);addr=("127.0.0.1",47999)
def c(m):
    s.sendto(m.encode(),addr);return s.recv(300).decode()
def move(deg,ms):
    counts=deg/SENS; n=max(1,int(ms/8)); t0=time.perf_counter()
    for i in range(n):
        c(f"mouse {counts/n:.3f} 0")
        tgt=t0+(i+1)*0.008
        while time.perf_counter()<tgt: time.sleep(0.0005)
moves=[(1,50),(2,100),(2,500),(5,60),(5,300),(10,100),(10,600),(20,150),(20,800),(45,200),(45,1000),(90,300),(90,1200)]
c("capture 1")
print(f"{'deg':>5} {'ms':>5} {'peak deg/s':>10} | {'measured':>18} | err deg  err %")
allerr=[]
for deg,ms in moves:
    got=[]; bads=[]
    for r in range(REPS):
        sign=1 if r%2==0 else -1
        c("track 1"); time.sleep(0.12); c("trackget")
        move(sign*deg,ms); time.sleep(0.45+0.2)
        px,fr,bad=c("trackget").split(); got.append(-float(px)/PX_PER_DEG*sign); bads.append(int(bad))
    c("track 0")
    e=[g-deg for g in got]; allerr+=[abs(x) for x in e]
    print(f"{deg:5} {ms:5} {deg/ms*1000:10.0f} | {st.mean(got):7.2f} ±{(st.pstdev(got)):5.2f}      | {st.mean(e):+6.2f}  {100*st.mean(e)/deg:+6.1f}%  lost frames {sum(bads)}")
print(f"mean absolute error {st.mean(allerr):.2f} deg, worst {max(allerr):.2f} deg")
c("capture 0")
