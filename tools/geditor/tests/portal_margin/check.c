#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include "bgdocument.h"
#include "bghistory.h"

void SetupFileFree(SetupFile *file) { (void)file; abort(); }
void StanFileFree(StanFile *file) { (void)file; abort(); }
#include "fixture.inc"
#include "input.inc"

static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir, BgFile *compiled)
{
    BgDocument loaded={0}; BgFile saved={0}; const char *why=""; char path[MAX_PATH];
    assert(BgDocumentCompile(doc,source,compiled,&why));
    /* Only the active table entry's margin changes, including shared polygons. */
    for (DWORD i=152;i<508;i++)
        if (i!=167) assert(compiled->data[i]==source->data[i]);
    assert(compiled->data[167]==doc->portals.portals[0].controlbytes2);
    snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,compiled,&why));
    assert(BgLoadProjectFile(dir,compiled->name,&saved,&why));
    assert(saved.size==compiled->size && !memcmp(saved.data,compiled->data,saved.size));
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
    assert(loaded.portals.portalcount==doc->portals.portalcount);
    assert(!memcmp(loaded.portals.portals,doc->portals.portals,doc->portals.portalcount*sizeof(BgPortal)));
    assert(loaded.facecount==doc->facecount && loaded.roomcount==doc->roomcount);
    BgDocumentFree(&loaded); BgFileFree(&saved);
}
int main(int argc, char **argv)
{
    assert(argc==2);
    unsigned char encoded=99;
    double parsed;
    assert(PortalPropertiesParseMargin(" 110.0 ",&parsed) && parsed==110);
    assert(PortalPropertiesParseMargin("0",&parsed) && parsed==0);
    assert(PortalPropertiesParseMargin("1.5e2",&parsed) && parsed==150);
    const char *invalid[]={""," ","-0.1","NaN","Inf","1e9999","2x","1 2"};
    for (unsigned int i=0;i<sizeof(invalid)/sizeof(*invalid);i++)
        assert(!PortalPropertiesParseMargin(invalid[i],&parsed));
    assert(!BgPortalEncodeMargin(NAN,&encoded) && encoded==99);
    assert(!BgPortalEncodeMargin(INFINITY,&encoded) && encoded==99);
    assert(!BgPortalEncodeMargin(-1,&encoded) && encoded==99);
    assert(!BgPortalEncodeMargin(122881,&encoded) && encoded==99);
    assert(BgPortalEncodeMargin(.125,&encoded) && encoded==1); /* Half-step rounds up. */
    assert(BgPortalEncodeMargin(24,&encoded) && encoded==0x3c);
    assert(BgPortalEncodeMargin(122880,&encoded) && encoded==255);
    BgFile source=Fixture(), compiled={0}, restored={0}; BgDocument doc={0};
    EditHistory history={0}; EditHistoryTransaction transaction={0}; EditHistoryAsset asset;
    SetupFile setup={0}; StanFile stan={0}; const char *why=""; BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,.21847887f,&doc,&why));
    BgPortal original[3]; memcpy(original,doc.portals.portals,sizeof(original));
    for (int i=0;i<256;i++) {
        BgPortal *portal=&doc.portals.portals[0]; portal->controlbytes2=(unsigned char)i;
        double native=BgPortalGetMargin(portal), world=native/(double)doc.levelscale;
        assert(BgPortalEncodeMargin(native,&encoded));
        BgPortal p={0}; p.controlbytes2=encoded; assert(BgPortalGetMargin(&p)==native);
        assert(BgDocumentSetPortalMargin(&doc,0,world,&changed,&why) && !changed && !doc.dirty);
        assert(portal->controlbytes2==i); /* Preserve every authored alias. */
    }
    memcpy(doc.portals.portals,original,sizeof(original));
    assert(!BgDocumentSetPortalMargin(&doc,3,110,&changed,&why) && !changed);
    assert(!BgDocumentSetPortalMargin(&doc,0,-1,&changed,&why) && !changed);
    assert(!BgDocumentSetPortalMargin(&doc,0,NAN,&changed,&why) && !changed);
    assert(!BgDocumentSetPortalMargin(&doc,0,1e300,&changed,&why) && !changed);
    assert(!doc.dirty && !memcmp(doc.portals.portals,original,sizeof(original)));
    EditHistoryReset(&history,&doc,&setup,&stan);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Change Portal Margin",&transaction,&why));
    assert(BgDocumentSetPortalMargin(&doc,0,110,&changed,&why) && changed && doc.dirty);
    assert(doc.portals.portals[0].controlbytes2==0x3c && BgPortalGetMargin(&doc.portals.portals[0])==24);
    BgPortal expected=original[0]; expected.controlbytes2=0x3c;
    assert(!memcmp(&expected,&doc.portals.portals[0],sizeof(expected)));
    assert(!memcmp(&original[1],&doc.portals.portals[1],2*sizeof(BgPortal)));
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&transaction,&why));
    RoundTrip(&doc,&source,argv[1],&compiled); EditHistoryMarkBgSaved(&history,&doc);
    assert(BgDocumentSetPortalMargin(&doc,0,110,&changed,&why) && !changed && !doc.dirty);
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why));
    assert(!memcmp(doc.portals.portals,original,sizeof(original)));
    RoundTrip(&doc,&compiled,argv[1],&restored); BgFileFree(&restored);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why));
    assert(doc.portals.portals[0].controlbytes2==0x3c);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Change Portal Margin",&transaction,&why));
    assert(BgDocumentSetPortalMargin(&doc,0,0,&changed,&why) && changed);
    assert(BgPortalGetMargin(&doc.portals.portals[0])==0);
    EditHistoryRollbackEdit(&transaction,&doc,&setup,&stan);
    assert(doc.portals.portals[0].controlbytes2==0x3c);
    EditHistoryFree(&history); BgDocumentFree(&doc); BgFileFree(&source); BgFileFree(&compiled);
    puts("PASS: margin input, all native encodings, nearest rounding, world scale, shared-portal isolation, save/reload, undo after save, redo and rollback.");
    return 0;
}
