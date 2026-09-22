#ifndef GEDITOR_LEVELSCALE_H
#define GEDITOR_LEVELSCALE_H
#include "bghistory.h"

/* Refresh world coordinates from native data, including retained undo/redo
 * documents. Does not alter authored bytes, identities or dirty revisions.
 * Validation precedes all mutation, so failure leaves every document intact. */
BOOL LevelScaleApply(BgDocument *bg, StanFile *stan, EditHistory *history,
                    BgDocument *faceclipboard, BgPortalFile *portalclipboard,
                    float scale, const char **why);
#endif
