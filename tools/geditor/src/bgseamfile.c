/* Editor-only seam guides. The ROM/BG format remains unchanged. Geometric
 * keys survive native compilation's vertex splits, compaction and face order. */
#include "bgdocument.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

typedef struct BgSeamKey { float p[6]; } BgSeamKey;
static int BgSeamCompare(const void *aa,const void *bb)
{
    const BgSeamKey *a=aa,*b=bb;
    for (int i=0;i<6;i++) if (a->p[i]!=b->p[i]) { return a->p[i]<b->p[i] ? -1 : 1; }
    return 0;
}
static BgSeamKey BgSeamKeyForEdge(const BgDocumentRoom *room,const BgDocumentFace *face,int corner)
{
    BgSeamKey key;
    for (int end=0;end<2;end++)
    {
        const BgDocumentVertex *v=&room->vertices[face->vertexindices[(corner+end)%3]];
        key.p[end*3]=room->origin[0]+v->x; key.p[end*3+1]=room->origin[1]+v->y; key.p[end*3+2]=room->origin[2]+v->z;
    }
    for (int k=0;k<3;k++) if (key.p[k]!=key.p[k+3])
    {
        if (key.p[k]>key.p[k+3]) for (int j=0;j<3;j++) { float t=key.p[j]; key.p[j]=key.p[j+3]; key.p[j+3]=t; }
        break;
    }
    return key;
}
static BOOL BgSeamPath(char path[MAX_PATH],const char *project,const char *name)
{
    const char *base=name;
    for (const char *p=name; *p; p++) { if (*p=='/' || *p=='\\') { base=p+1; } }
    if (!*base) { return FALSE; }
    int n=snprintf(path,MAX_PATH,"%s\\bg\\%s.uvseams",project,base);
    return n>0 && n<MAX_PATH;
}
BOOL BgDocumentSaveSeams(const BgDocument *doc,const char *project,const char *name,const char **why)
{
    char path[MAX_PATH],temp[MAX_PATH]; DWORD count=0;
    *why="Could not save the UV seam guides.";
    if (!BgSeamPath(path,project,name) || snprintf(temp,sizeof(temp),"%s.tmp",path)>=(int)sizeof(temp)) { return FALSE; }
    for (DWORD r=1;r<=doc->roomcount;r++) for (DWORD f=0;f<doc->rooms[r].facecount;f++)
    { unsigned char mask=doc->rooms[r].faces[f].uvseams; for (int c=0;c<3;c++) { count+=(mask>>c)&1; } }
    FILE *file=fopen(temp,"wb"); if (!file) { return FALSE; }
    BOOL ok=fprintf(file,"GEDITOR_UV_SEAMS 1 %lu\n",(unsigned long)count)>0;
    for (DWORD r=1;r<=doc->roomcount && ok;r++) for (DWORD f=0;f<doc->rooms[r].facecount && ok;f++)
    {
        const BgDocumentFace *face=&doc->rooms[r].faces[f];
        for (int c=0;c<3 && ok;c++) if (face->uvseams&(1<<c))
        {
            BgSeamKey key=BgSeamKeyForEdge(&doc->rooms[r],face,c);
            ok=fprintf(file,"%.9g %.9g %.9g %.9g %.9g %.9g\n",key.p[0],key.p[1],key.p[2],key.p[3],key.p[4],key.p[5])>0;
        }
    }
    if (fclose(file)) { ok=FALSE; }
    if (ok) { ok=MoveFileEx(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH); }
    if (!ok) { DeleteFile(temp); return FALSE; }
    *why=""; return TRUE;
}
BOOL BgDocumentLoadSeams(BgDocument *doc,const char *project,const char *name,const char **why)
{
    char path[MAX_PATH],magic[32]; unsigned version,count;
    *why="Could not read the UV seam guides.";
    if (!BgSeamPath(path,project,name)) { return FALSE; }
    FILE *file=fopen(path,"rb");
    if (!file) { if (errno==ENOENT) { *why=""; return TRUE; } return FALSE; }
    BgSeamKey *keys=NULL; BOOL ok=FALSE;
    if (fscanf(file,"%31s %u %u",magic,&version,&count)!=3 || strcmp(magic,"GEDITOR_UV_SEAMS")
        || version!=1 || count>3000000) { goto done; }
    keys=calloc(count?count:1,sizeof(*keys)); if (!keys) { goto done; }
    for (unsigned i=0;i<count;i++)
    {
        float *p=keys[i].p;
        if (fscanf(file,"%f %f %f %f %f %f",p,p+1,p+2,p+3,p+4,p+5)!=6) { goto done; }
        for (int k=0;k<6;k++) if (!isfinite(p[k])) { goto done; }
    }
    char extra; if (fscanf(file," %c",&extra)!=EOF || ferror(file)) { goto done; }
    qsort(keys,count,sizeof(*keys),BgSeamCompare);
    for (DWORD r=1;r<=doc->roomcount;r++) for (DWORD f=0;f<doc->rooms[r].facecount;f++)
    {
        BgDocumentFace *face=&doc->rooms[r].faces[f]; face->uvseams=0;
        for (int c=0;c<3;c++)
        {
            BgSeamKey key=BgSeamKeyForEdge(&doc->rooms[r],face,c);
            if (bsearch(&key,keys,count,sizeof(*keys),BgSeamCompare)) { face->uvseams|=(unsigned char)(1<<c); }
        }
    }
    *why=""; ok=TRUE;
done:
    free(keys); fclose(file); return ok;
}
