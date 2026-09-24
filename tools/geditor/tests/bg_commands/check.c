#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgcommands.h"
#define OK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s: %s\n",__LINE__,#x,why);abort(); } } while (0)
static const char *why="";
static DWORD R(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void W(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static DWORD Cell(unsigned char *p,DWORD at,unsigned type,unsigned length,DWORD arg)
{ p[at]=type;p[at+1]=length;p[at+2]=p[at+3]=0;W(p+at+4,arg);return at+8; }
static void Synthetic(void)
{
    unsigned char data[8192]={0},before[8192];BgFile bg={0};BgVisProgram p={0};
    BgPortal list[2]={{.geometryoffset=0x300,.connectedroom1=1,.connectedroom2=2},
                      {.geometryoffset=0x300,.connectedroom1=3,.connectedroom2=4}};
    BgPortalFile portals={list,2};bg.data=data;bg.size=sizeof(data);W(data+12,0x0f000080);
    DWORD at=0x80;
    at=Cell(data,at,0x14,3,0);at=Cell(data,at,0x65,0,1);at=Cell(data,at,0x65,0,3);
    at=Cell(data,at,0x5a,1,0);
    at=Cell(data,at,0x1f,2,0);at=Cell(data,at,0x64,0,0x0f000300);
    at=Cell(data,at,0x20,2,0);at=Cell(data,at,0x65,0,5);
    at=Cell(data,at,0x5b,1,0);at=Cell(data,at,0x1e,1,0);at=Cell(data,at,0x5c,1,0);at=Cell(data,at,0,0,0);
    memcpy(before,data,sizeof(data));OK(BgVisDecode(&bg,&portals,5,&p,&why));
    OK(p.complete && p.count==8 && !p.warnings && !p.problem[0] && p.size==at-0x80);
    OK(p.instructions[0].arg[0]==1 && p.instructions[0].arg[1]==3);
    OK(p.instructions[2].portal==0 && p.instructions[2].matches==2 && p.instructions[2].room1==1);
    OK(p.instructions[1].depth==0 && p.instructions[2].depth==1 && p.instructions[4].depth==0 && p.instructions[5].depth==1 && p.instructions[6].depth==0);
    char *report=BgVisReport(&p,"Fixture");OK(report && strstr(report,"Rooms 1 through 3") && strstr(report,"Portal 0") && strstr(report,"64 00 00 00  0F 00 03 00"));free(report);BgVisFree(&p);
    OK(!memcmp(data,before,sizeof(data)));
    /* Live portal edits: a removed first connection changes the native first match. */
    portals.portals=list+1;portals.portalcount=1;
    OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.instructions[2].portal==0 && p.instructions[2].room1==3 && p.instructions[2].matches==1);BgVisFree(&p);
    portals.portalcount=0;OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.warnings==1 && (p.instructions[2].warnings&BG_VIS_PORTAL));BgVisFree(&p);
    list[0].geometryoffset=BG_PORTAL_NEW_GEOMETRY|3;bg.newportaloffsets[3]=0x300;portals.portals=list;portals.portalcount=1;
    OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.instructions[2].portal==0 && !p.warnings);BgVisFree(&p);
    /* Literal portal indices use value cells rather than polygon relocation. */
    data[0xa8]=0x65;W(data+0xac,0);OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.instructions[2].portal==0 && !p.warnings);BgVisFree(&p);
    memcpy(data,before,sizeof(data));
    W(data+0x8c,7);OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.instructions[0].warnings&BG_VIS_ROOM);BgVisFree(&p);
    memcpy(data,before,sizeof(data));data[0x81]=0;
    OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(!p.complete && p.problem[0] && p.count==1 && p.size==8);BgVisFree(&p);
    memcpy(data,before,sizeof(data));bg.size=0x90;
    OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(!p.complete && p.problem[0]);BgVisFree(&p);bg.size=sizeof(data);
    memcpy(data,before,sizeof(data));data[0x88]=0;
    OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(!p.complete && p.problem[0] && (p.instructions[0].warnings&BG_VIS_OPERAND));BgVisFree(&p);
    memset(data+0x80,0,sizeof(data)-0x80);Cell(data,0x80,0x7f,1,0);Cell(data,0x88,0,0,0);
    OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.complete && p.warnings==1 && !strcmp(BgVisName(0x7f),"UNKNOWN"));BgVisFree(&p);
    memset(data+0x80,0,sizeof(data)-0x80);at=0x80;
    for (unsigned i=0;i<=BG_VIS_NESTING_LIMIT;i++) { at=Cell(data,at,0x5a,1,0); }
    OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(!p.complete && strstr(p.problem,"nesting"));BgVisFree(&p);
    W(data+12,0x0fffffff);OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.problem[0] && !p.count);BgVisFree(&p);
    W(data+12,0);OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(!p.present && p.complete);BgVisFree(&p);
    W(data,0x0f000080);OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.singleDisplayList && !p.present);BgVisFree(&p);
    OK(!BgVisDecode(NULL,NULL,0,&p,&why));
    /* Every supported opcode is named, decoded, and can produce a report. */
    unsigned types[]={0,1,2,3,4,5,6,0x14,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x50,0x51,0x52,0x5a,0x5b,0x5c};
    W(data,0);W(data+12,0x0f000080);
    for (unsigned j=0;j<sizeof(types)/sizeof(*types);j++)
    {
        unsigned op=types[j],length=(op==0x14 || op==0x25 || op==0x27) ? 3 : (op==0x1f || op==0x20 || (op>=0x22 && op<=0x24) || op==0x26) ? 2 : 1;
        memset(data+0x80,0,sizeof(data)-0x80);Cell(data,0x80,op,length,0xffffffffu);
        for (unsigned a=1;a<length;a++) { Cell(data,0x80+a*8,0x65,0,1); }
        OK(BgVisDecode(&bg,&portals,5,&p,&why));OK(p.count && !(p.instructions[0].warnings&BG_VIS_UNKNOWN));
        report=BgVisReport(&p,"Opcode");OK(report && strstr(report,BgVisName(op)));free(report);BgVisFree(&p);
    }
    puts("PASS: visibility decoding, indentation, polygon aliases/literal indices, live portal changes, read-only input, every opcode, malformed and truncated data, nesting limits, and copy reports.");
}
static void Corpus(const char *path)
{
    FILE *f=fopen(path,"rb");OK(f);OK(!fseek(f,0,SEEK_END));long length=ftell(f);OK(length>=20);rewind(f);
    BgFile bg={0};bg.size=(DWORD)length;bg.data=malloc(bg.size);OK(bg.data && fread(bg.data,1,bg.size,f)==bg.size);fclose(f);
    unsigned char *original=malloc(bg.size);OK(original);memcpy(original,bg.data,bg.size);
    BgPortalFile portals={0};BgVisProgram p={0};OK(BgLoadPortals(bg.data,bg.size,1,&portals,&why));
    DWORD table=R(bg.data+4)&0xffffffu,rooms=0;
    for (DWORD r=1;r<256;r++) { OK(table+r*24+24<=bg.size);if (!R(bg.data+table+r*24+4)) { break; }rooms=r; }
    OK(BgVisDecode(&bg,&portals,rooms,&p,&why));OK(p.complete && !p.problem[0] && !p.warnings);
    if (strstr(path,"bg_dam_all_p")) { OK(p.count==203 && p.size==3112 && p.instructions[0].arg[0]==127 && p.instructions[0].arg[1]==131); }
    if (strstr(path,"bg_arec_all_p")) { OK(p.count==47 && p.size==648 && p.instructions[0].opcode==0x21 && p.instructions[1].arg[0]==30 && p.instructions[1].arg[1]==74); }
    for (DWORD i=0;i<p.count;i++) if (p.instructions[i].argument==BG_VIS_ONE_PORTAL)
    {
        DWORD polygon=p.instructions[i].arg[0]&0xffffffu,index=p.instructions[i].portal;
        OK(index<portals.portalcount && portals.portals[index].geometryoffset==polygon);
        for (DWORD earlier=0;earlier<index;earlier++) { OK(portals.portals[earlier].geometryoffset!=polygon); }
    }
    char *report=BgVisReport(&p,path);OK(report && strstr(report,"END [0x00]"));free(report);
    OK(!memcmp(original,bg.data,bg.size));printf("PASS: %s: %lu instructions, %lu bytes, all room and portal references resolved.\n",path,(unsigned long)p.count,(unsigned long)p.size);
    /* The normal save/export compactor relocates polygon and script pointers.
     * The inspector must still resolve identical portal indices afterward. */
    BgFile packed={0};BgPortalFile packedportals={0};BgVisProgram after={0};OK(BgFileCompact(&bg,&packed,&why));
    OK(BgLoadPortals(packed.data,packed.size,1,&packedportals,&why));OK(BgVisDecode(&packed,&packedportals,rooms,&after,&why));
    OK(after.complete && !after.warnings && p.count==after.count);
    for (DWORD n=0;n<p.count;n++)
    {
        const BgVisInstruction *a=p.instructions+n,*b=after.instructions+n;OK(a->opcode==b->opcode && a->length==b->length && a->depth==b->depth);
        if (a->argument==BG_VIS_ONE_PORTAL) { OK(a->portal==b->portal && a->room1==b->room1 && a->room2==b->room2); }
        else { OK(!memcmp(a->arg,b->arg,sizeof(a->arg))); }
    }
    BgVisFree(&after);BgPortalFileFree(&packedportals);BgFileFree(&packed);
    BgVisFree(&p);BgPortalFileFree(&portals);free(original);BgFileFree(&bg);
}
int main(int argc,char **argv) { Synthetic();for (int i=1;i<argc;i++) { Corpus(argv[i]); }return 0; }
