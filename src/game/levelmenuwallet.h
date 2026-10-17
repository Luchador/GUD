#ifndef GUD_LEVELMENUWALLET_H
#define GUD_LEVELMENUWALLET_H
#include "../levelmenuassets.h"
/* Work on the native, unpromoted model before texture expansion. All reads and
 * replacement sites are checked before the first write. No live RDP list edits. */
static unsigned levelWalletRead(const unsigned char *p)
{ return (unsigned)p[0]<<24 | (unsigned)p[1]<<16 | (unsigned)p[2]<<8 | p[3]; }
static void levelWalletWrite(unsigned char *p,unsigned v)
{ p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static int levelWalletPointer(const unsigned char *data,unsigned size,unsigned at,unsigned bytes,unsigned *out)
{
    unsigned value;
    if (at>size || size-at<4) return 0;
    value=levelWalletRead(data+at);
    if ((value>>24)!=5) return 0;
    *out=value&0xffffff;
    return *out<=size && bytes<=size-*out;
}
static int levelWalletPatch(unsigned char *data,unsigned size,unsigned switches,unsigned textures,
    const LevelMenuAsset *assets,const int order[20],unsigned *defaults)
{
    unsigned offsets[60],values[60],count=0,first=size,i,j,n,d,g,slot,seen,opcode,id,unique=0;
    unsigned preload[256],pending[512],visited[256],todo=1,visitedCount=0;
    if (!data || switches<43 || switches>256 || textures>256 || size<switches*4+textures*12) return 0;
    for (i=0;i<21;i++)
    {
        slot=i ? i-1 : 0;
        if (i && (order[slot]<0 || order[slot]>=20)) return 0;
        if (!levelWalletPointer(data,size,(i ? 21+i : 21)*4,24,&n)) return 0;
        opcode=(data[n]<<8)|data[n+1];
        if (i) {
            if (opcode!=18 || !levelWalletPointer(data,size,n+20,24,&n)) return 0;
            opcode=(data[n]<<8)|data[n+1];
        }
        if ((opcode!=4 && opcode!=24) || !levelWalletPointer(data,size,n+4,20,&d)
            || !levelWalletPointer(data,size,d,8,&g) || (g&7)) return 0;
        seen=0;
        for (;g<=size-8;g+=8) {
            opcode=data[g];
            if (opcode==0xb8) break;
            if (opcode==0xc0) {
                unsigned field=i ? LEVEL_ASSET_BRIEF_TOP+seen : LEVEL_ASSET_MENU_IMAGE;
                unsigned target=i ? order[slot] : (seen<20 ? order[seen] : 20);
                if (target>=20 || (i && seen>=2) || (levelWalletRead(data+g)&7)!=4) return 0;
                if (defaults) defaults[target*3+field]=levelWalletRead(data+g+4)&0xfff;
                if (assets[target].flags&LEVEL_ASSET_BIT(field)) {
                    if (count>=60 || (unsigned)assets[target].images[field]>=4096) return 0;
                    offsets[count]=g+4;values[count++]=assets[target].images[field];
                }
                seen++;
            }
        }
        if (g>size-8 || seen!=(i ? 2u : 20u)) return 0;
    }
    if (!count) return 1;
    /* Follow the bounded native graph, never scan vertex bytes for opcodes. */
    pending[0]=switches*4+textures*12;
    while (todo) {
        n=pending[--todo];
        if (n>size || size-n<24) return 0;
        for (j=0;j<visitedCount && visited[j]!=n;j++) {}
        if (j<visitedCount) continue;
        if (visitedCount>=256) return 0;
        visited[visitedCount++]=n;
        opcode=(data[n]<<8)|data[n+1];
        if (opcode==4 || opcode==24) {
            if (!levelWalletPointer(data,size,n+4,20,&d)) return 0;
            for (j=0;j<2;j++) if (levelWalletRead(data+d+j*4)) {
                if (!levelWalletPointer(data,size,d+j*4,8,&g) || (g&7)) return 0;
                if (g<first) first=g;
            }
        }
        for (j=12;j<=20;j+=8) if (levelWalletRead(data+n+j)) {
            if (todo>=512 || !levelWalletPointer(data,size,n+j,24,&g)) return 0;
            pending[todo++]=g;
        }
    }
    if (first==size || (first&7)) return 0;
    for (i=first;i+8<=size;i+=8) if (data[i]==0xc0) {
        if ((levelWalletRead(data+i)&7)>4) return 0;
        id=levelWalletRead(data+i+4)&0xfff;
        for (j=0;j<count;j++) if (offsets[j]==i+4) {id=values[j];break;}
        for (j=0;j<unique && preload[j]!=id;j++) {}
        if (j==unique) {if (unique>=256) return 0;preload[unique++]=id;}
        /* Wallet material markers are single-texture, never detail. */
        if ((levelWalletRead(data+i)&7)==1) return 0;
    }
    for (i=0;i<count;i++) levelWalletWrite(data+offsets[i],(levelWalletRead(data+offsets[i])&~0xfffu)|values[i]);
    /* Preload only referenced IDs; replacing images must not keep unused
     * photographs alive. The wallet has more markers than preload slots
     * (All Bonds portraits already do this); remaining IDs load on demand. */
    for (i=0;i<textures;i++) levelWalletWrite(data+switches*4+i*12,i<unique ? preload[i] : 0xffffffffu);
    return 1;
}
#endif
