#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stanload.h"

static void Point(StanFile *s,int t,int p,int x,int y,int z)
{ s->tiles[t].points[p]=(StanPoint){x/s->levelscale,y/s->levelscale,z/s->levelscale,0}; }
static void Link(StanFile *s,int t,int p,int other)
{ s->tiles[t].points[p].link=other<0?0:(s->tiles[other].sourceoffset-s->tiles[0].sourceoffset+0x80)/8; }
static void Check(const StanFile *s,DWORD expected,int low,int high)
{
    StanTile before[2];assert(s->tilecount==2);memcpy(before,s->tiles,sizeof(before));
    StanDiscontinuity *gaps=NULL;DWORD count=0;
    assert(StanBuildDiscontinuities(s,&gaps,&count) && count==expected);
    assert(!memcmp(before,s->tiles,sizeof(before)));
    for(DWORD i=0;i<count;i++)
    {
        assert(gaps[i].tiles[0]!=gaps[i].tiles[1]);
        assert(lroundf(gaps[i].ends[0].z*s->levelscale)==low);
        assert(lroundf(gaps[i].ends[1].z*s->levelscale)==high);
    }
    free(gaps);
}
static void Pair(StanFile *s)
{
    memset(s->tiles,0,2*sizeof(*s->tiles));
    for(int t=0;t<2;t++) { s->tiles[t].pointcount=4;s->tiles[t].sourceoffset=12+t*40; }
    Point(s,0,0,-20,0,-20);Point(s,0,1,-20,0,20);Point(s,0,2,0,0,20);Point(s,0,3,0,0,-20);
    Point(s,1,0,0,0,-20);Point(s,1,1,0,0,20);Point(s,1,2,20,10,20);Point(s,1,3,20,10,-20);
}
int main(void)
{
    StanTile tiles[2];StanFile s={.tiles=tiles,.tilecount=2,.levelscale=1};
    const float scales[]={.25f,.49886572f,1,2};
    for(unsigned k=0;k<sizeof(scales)/sizeof(*scales);k++)
    {
        s.levelscale=scales[k];Pair(&s);Check(&s,1,-20,20);
        Link(&s,0,2,1);Check(&s,1,-20,20); /* One-way link. */
        Link(&s,1,0,0);Check(&s,0,0,0);
        Link(&s,1,0,1);Check(&s,1,-20,20); /* Wrong destination. */
        Pair(&s);Point(&s,1,0,0,1,-20);Point(&s,1,1,0,1,20);Check(&s,0,0,0); /* Stacked floor. */
        Pair(&s);Point(&s,1,0,1,0,-20);Point(&s,1,1,1,0,20);Check(&s,0,0,0); /* Native-unit gap. */
        Pair(&s);Point(&s,1,0,0,0,20);Point(&s,1,1,0,0,40);Check(&s,0,0,0); /* Point-only touch. */
        Pair(&s);Point(&s,1,0,0,0,-10);Point(&s,1,1,0,0,10);Check(&s,1,-10,10);
        /* Subdivided doorway boundary: the middle is linked, both ends are not. */
        Pair(&s);tiles[1].pointcount=6;
        Point(&s,1,0,0,0,-20);Point(&s,1,1,0,0,-10);Point(&s,1,2,0,0,10);
        Point(&s,1,3,0,0,20);Point(&s,1,4,20,0,20);Point(&s,1,5,20,0,-20);
        Link(&s,0,2,1);Link(&s,1,1,0);
        StanDiscontinuity *gaps=NULL;DWORD count;
        assert(StanBuildDiscontinuities(&s,&gaps,&count) && count==2);
        assert(lroundf(gaps[0].ends[0].z*s.levelscale)==-20 && lroundf(gaps[0].ends[1].z*s.levelscale)==-10);
        assert(lroundf(gaps[1].ends[0].z*s.levelscale)==10 && lroundf(gaps[1].ends[1].z*s.levelscale)==20);
        free(gaps);Link(&s,1,0,0);Link(&s,1,2,0);Check(&s,0,0,0);
        /* A long boundary need not begin at the same point as its neighbor. */
        Link(&s,0,2,-1);assert(StanBuildDiscontinuities(&s,&gaps,&count) && count==3);free(gaps);
        /* Exact vertical/sloping lines, in either perimeter orientation. */
        for(int direction=0;direction<2;direction++)
        {
            Pair(&s);tiles[0].pointcount=tiles[1].pointcount=3;
            Point(&s,0,0,-32768,-30000,-32768);Point(&s,0,1,32767,30000,32767);Point(&s,0,2,-10,300,100);
            Point(&s,1,direction,32767,30000,32767);Point(&s,1,1-direction,-32768,-30000,-32768);Point(&s,1,2,10,-300,-100);
            Check(&s,1,-32768,32767);
            for(int t=0;t<2;t++)for(int p=0;p<2;p++)tiles[t].points[p].x=tiles[t].points[p].z=0;
            assert(StanBuildDiscontinuities(&s,&gaps,&count) && count==1);
            assert(lroundf(gaps[0].ends[0].y*s.levelscale)==-30000 && lroundf(gaps[0].ends[1].y*s.levelscale)==30000);
            free(gaps);
        }
        Pair(&s);tiles[0].points[2]=tiles[0].points[3];Check(&s,0,0,0); /* Collapsed edge. */
    }
    StanDiscontinuity *gaps=(void *)1;DWORD count=42;
    assert(StanBuildDiscontinuities(NULL,&gaps,&count) && !gaps && !count);
    s.levelscale=NAN;assert(!StanBuildDiscontinuities(&s,&gaps,&count) && !gaps && !count);
    puts("PASS: missing/one-way/wrong links, partial and subdivided overlaps, repair rebuilds, native scale/height separation, point contacts, slopes/verticals, signed-short limits and unchanged source tiles.");
    return 0;
}
