#ifndef _MODELLOD_H_
#define _MODELLOD_H_

#include "model.h"

/* Scoped to chrTick's ordinary body pose. Other model users retain all matrices. */
extern Model *g_ModelLodModel;
void modelLodBegin(Model *model);
void modelLodEnd(void);
bool modelLodSkipJoint(Model *model, ModelRoData_GroupRecord *group);
void modelLodResetCache(void);
/* Call before replacing per-instance display lists or resetting runtime data. */
void modelLodInvalidateInstance(Model *model);

#endif
