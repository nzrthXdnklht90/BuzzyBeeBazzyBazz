import numpy as np, itertools
fs=48000.0
def mag(kind,f0,gdb,k,hz):
    t=np.tan(np.pi*min(f0,0.49*fs)/fs)
    A=10**(gdb/40)
    if kind=='ls': g=t/np.sqrt(A); m0,m1,m2=1,k*(A-1),A*A-1
    w=np.tan(np.pi*hz/fs)/g
    reD=1-w*w; imD=k*w
    reN=m0*reD+m2; imN=m0*imD+m1*w
    return 20*np.log10(np.sqrt((reN**2+imN**2)/(reD**2+imD**2)))
def resp(b,a,F,P,hz):
    bm,bq,am,aq,ag=P
    return mag('ls',F*bm,1.35*b,1/bq,hz)+mag('ls',F*am,-ag*a,1/aq,hz)
best=None
for bm,bq,am,aq,ag in itertools.product([0.8,1.0,1.2,1.4],[0.7,0.9,1.2,1.5],[1.5,2,2.5,3,4],[0.35,0.5,0.7],[0.9,1.1,1.3,1.5,1.75]):
    P=(bm,bq,am,aq,ag)
    e=0
    e+=(resp(5,5,60,P,50)-3)**2
    r=[resp(5,5,60,P,h) for h in (150,200,250,300,400)]; e+=(min(r)+1.5)**2
    e+=(resp(5,0,60,P,40)-6.5)**2
    e+=(resp(0,5,60,P,30)+6)**2
    e+=(resp(10,10,30,P,25)-4)**2
    r=[resp(10,10,30,P,h) for h in (80,100,120,150,200)]; e+=0.5*(min(r)+5)**2
    if best is None or e<best[0]: best=(e,P)
print(best)
P=best[1]
for s in [(5,5,60),(5,0,60),(0,5,60),(10,10,30)]:
    print(s,[round(resp(s[0],s[1],s[2],P,h),1) for h in (20,30,40,60,100,150,250,400,1000)])
