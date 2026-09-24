#ifndef GEDITOR_SETUPMETA_H
#define GEDITOR_SETUPMETA_H
#include <windows.h>
#include <string.h>
/* Optional project-only trailer. Native setup bytes remain an unchanged prefix.
 * Load splits Action Block and briefing metadata from the native prefix.
 * ROM export compiles them into a separate copy, discarding editor metadata.
 * One atomic project-file replacement keeps scripts and their names together. */
#define SETUP_META_MAGIC "GEDAI001"
#define SETUP_META_FOOTER 16u
#define SETUP_META_BRIEF_MAGIC "GEDSET02"
#define SETUP_META_BRIEF_FOOTER 20u
#define SETUP_META_MAX (4u * 1024u * 1024u)
static inline DWORD SetupMetaRead32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static inline void SetupMetaWrite32(unsigned char *p, DWORD value)
{ p[0]=value>>24; p[1]=value>>16; p[2]=value>>8; p[3]=value; }
static inline BOOL SetupMetaSplitAll(const unsigned char *data, DWORD size, DWORD *native, DWORD *meta, DWORD *brief)
{
    *native=size; *meta=0; *brief=0;
    if (size>=SETUP_META_BRIEF_FOOTER && !memcmp(data+size-SETUP_META_BRIEF_FOOTER,SETUP_META_BRIEF_MAGIC,8))
    {
        DWORD n=SetupMetaRead32(data+size-12), a=SetupMetaRead32(data+size-8), b=SetupMetaRead32(data+size-4);
        if (n<40 || n>size-20 || a>SETUP_META_MAX || b>SETUP_META_MAX-a
            || a>size-20-n || b!=size-20-n-a) { return FALSE; }
        *native=n; *meta=a; *brief=b; return TRUE;
    }
    if (size>=SETUP_META_FOOTER && !memcmp(data+size-SETUP_META_FOOTER,SETUP_META_MAGIC,8))
    {
        DWORD n=SetupMetaRead32(data+size-8), m=SetupMetaRead32(data+size-4);
        if (n<40 || m>SETUP_META_MAX || n>size-SETUP_META_FOOTER
            || m!=size-SETUP_META_FOOTER-n) { return FALSE; }
        *native=n; *meta=m;
    }
    return TRUE;
}
static inline BOOL SetupMetaSplit(const unsigned char *data, DWORD size, DWORD *native, DWORD *meta)
{ DWORD brief; return SetupMetaSplitAll(data,size,native,meta,&brief); }
#endif
