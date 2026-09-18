#ifndef GEDITOR_SETUPMETA_H
#define GEDITOR_SETUPMETA_H
#include <windows.h>
#include <string.h>
/* Optional project-only trailer. Native setup bytes remain an unchanged prefix.
 * Load splits it into SetupFile.actionmeta; ROM export applies disabled block
 * flags to a separate native copy, then discards all editor metadata.
 * One atomic project-file replacement keeps scripts and their names together. */
#define SETUP_META_MAGIC "GEDAI001"
#define SETUP_META_FOOTER 16u
#define SETUP_META_MAX (4u * 1024u * 1024u)
static inline DWORD SetupMetaRead32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static inline void SetupMetaWrite32(unsigned char *p, DWORD value)
{ p[0]=value>>24; p[1]=value>>16; p[2]=value>>8; p[3]=value; }
static inline BOOL SetupMetaSplit(const unsigned char *data, DWORD size, DWORD *native, DWORD *meta)
{
    *native=size; *meta=0;
    if (size>=SETUP_META_FOOTER && !memcmp(data+size-SETUP_META_FOOTER,SETUP_META_MAGIC,8))
    {
        DWORD n=SetupMetaRead32(data+size-8), m=SetupMetaRead32(data+size-4);
        if (n<40 || m>SETUP_META_MAX || n>size-SETUP_META_FOOTER
            || m!=size-SETUP_META_FOOTER-n) { return FALSE; }
        *native=n; *meta=m;
    }
    return TRUE;
}
#endif
