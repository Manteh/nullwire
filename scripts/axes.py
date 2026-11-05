#!/usr/bin/env python3
# Real speed of each stick direction, measured with the 2D tracker. Horizontal speed sets the pixel scale.
import socket,time,sys,math,statistics as st
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.settimeout(10);addr=("127.0.0.1",47999)
def c(m):
    s.sendto(m.encode(),addr);r=s.recv(300).decode()
    if r in("bad command","unknown"): sys.exit("REJECTED "+m)
    return r
def run(x,y,ms):
    c("t2d 1");time.sleep(.2);c(f"hold2 {x} {y} {ms}");time.sleep(.5);px,py,fr,bad=c("t2dget").split();c("t2d 0");return float(px),float(py),int(bad)
WMAX=float(sys.argv[1]);DZ=float(sys.argv[2])
c("capture 1")
print("sanity, still camera:",run(0,0,500))
fs=[]
for sgn in(1,-1,1,-1,1,-1):
    px,py,b=run(sgn*.5,0,400); deg=(.5-DZ)/(1-DZ)*WMAX*.4; fs.append(abs(px)/deg)
pxdeg=st.mean(fs); print(f"horizontal scale: {pxdeg:.3f} px/deg in a 320 wide picture (spread {max(fs)-min(fs):.3f}), view width about {2*math.degrees(math.atan(160/(pxdeg*180/math.pi))):.0f} deg")
print("\nstick x   stick y |  yaw deg/s  pitch deg/s |  direction want / got")
rows=[(.5,0),(.3,0),(.2,0),(0,.5),(0,-.5),(0,.3),(0,-.3),(0,.2),(0,-.2),(.354,.354),(.354,-.354),(-.354,.354),(.2,.2),(.5,.1),(.1,.5),(.14,0),(0,.14),(.11,0),(0,.11),(.09,0),(0,.09)]
for x,y in rows:
    ms=350;a=[];b=[]
    for sg in(1,-1):
        px,py,bad=run(sg*x,sg*y,ms); a.append(-px*sg/pxdeg/(ms/1000)); b.append(py*sg/pxdeg/(ms/1000))
    yr,pr=st.mean(a),st.mean(b)
    want=math.degrees(math.atan2(y,x)); got=math.degrees(math.atan2(pr,yr)) if abs(yr)+abs(pr)>1 else float('nan')
    print(f"{x:7.3f} {y:7.3f}  | {yr:9.1f} {pr:11.1f}   | {want:7.1f} / {got:7.1f}",flush=True)
c("capture 0")
