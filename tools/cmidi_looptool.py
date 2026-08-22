import struct
class T:
    def __init__(s,d,pos):
        s.d=d; s.pos=pos; s.bu=0; s.bulen=0; s.last=0; s.escapes=[]
    def rb(s):
        if s.bulen: b=s.d[s.bu]; s.bu+=1; s.bulen-=1; return b
        b=s.d[s.pos]; s.pos+=1
        if b==0xFE:
            nxt=s.d[s.pos]; s.pos+=1
            if nxt!=0xFE:
                hi,lo,ln=nxt,s.d[s.pos],s.d[s.pos+1]; s.pos+=2
                backup=(hi<<8)|lo
                src=s.pos-backup-4
                s.escapes.append({'site':s.pos-4,'src':src,'backup':backup,'len':ln})
                s.bu=src; s.bulen=ln
                return s.rb()
            return 0xFE
        return b
    def varlen(s):
        v=s.rb(); n=v&0x7F
        while v&0x80: v=s.rb(); n=(n<<7)|(v&0x7F)
        return n
def parse(fn):
    d=open(fn,'rb').read()
    offs=list(struct.unpack('>16I',d[:64]))
    tracks=[]
    for ti,off in enumerate(offs):
        if not off: continue
        t=T(d,off); loopstarts=[]; loopends=[]; endpos=None; n=0
        while True:
            dpos=t.pos
            t.varlen()
            st=t.rb(); n+=1
            if st==0xFF:
                ty=t.rb()
                if ty==0x51: t.rb();t.rb();t.rb()
                elif ty==0x2F: endpos=(dpos,t.pos); break
                elif ty==0x2E: t.rb();t.rb(); loopstarts.append({'delta':dpos,'after':t.pos})
                elif ty==0x2D:
                    ct=t.rb();cur=t.rb()
                    o=(t.rb()<<24)|(t.rb()<<16)|(t.rb()<<8)|t.rb()
                    loopends.append({'delta':dpos,'end':t.pos,'ct':ct,'off':o,'target':t.pos-o})
                t.last=0
            else:
                if st&0x80: eff=st; t.last=st; b1=t.rb()
                else: eff=t.last
                hi=eff&0xF0
                if hi not in (0xC0,0xD0):
                    t.rb()
                    if hi==0x90: t.varlen()
        tracks.append({'ti':ti,'off':off,'events':n,'ls':loopstarts,'le':loopends,'end':endpos,'esc':t.escapes})
    return d,offs,tracks
if __name__=='__main__':
    import sys
    for fn in sys.argv[1:]:
        d,offs,tr=parse(fn)
        print(f"== {fn} ({len(d)}B, {len(tr)} tracks)")
        for t in tr:
            tgt_ok=""
            if t['le']:
                afters={l['after'] for l in t['ls']}
                tgt_ok=" targets->" + ",".join("AFTER-LS" if l['target'] in afters else ("DELTA-LS" if l['target'] in {x['delta'] for x in t['ls']} else f"?{l['target']}") for l in t['le'])
            print(f"  trk{t['ti']:2d} off=0x{t['off']:04x} ev={t['events']:4d} LS={len(t['ls'])} LE={len(t['le'])} esc={len(t['esc'])} end@{t['end'][0] if t['end'] else '?'}{tgt_ok}")
