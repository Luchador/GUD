/* Frozen pre-patch list builder: ordering and pool-exhaustion oracle. */
ModelHitEntry *referenceBuildNodeList(ModelHitEntry *head, Model *model)
{
    ModelHitEntry *firstNewEntry = g_ModelHitFreeList;
    ModelHitEntry *freeEntry = firstNewEntry;
    ModelNode *node = model->obj->RootNode;
    ModelNode *nextNode;

    while (node != NULL && freeEntry != NULL)
    {
        switch (node->Opcode & 0xff)
        {
            case MODELNODE_OPCODE_HEADER:
            case MODELNODE_OPCODE_GROUP:
            case MODELNODE_OPCODE_OP03:
            case MODELNODE_OPCODE_OP11:
            case MODELNODE_OPCODE_GUNFIRE:
            case MODELNODE_OPCODE_SHADOW:
            case MODELNODE_OPCODE_OP14:
            case MODELNODE_OPCODE_INTERLINK:
            case MODELNODE_OPCODE_OP16:
            case MODELNODE_OPCODE_GROUPSIMPLE:
                freeEntry->model = model;
                freeEntry->rootnode = node;
                freeEntry = freeEntry->next;
                break;
            default:
                break;
        }

        if (node->Child != NULL)
        {
            node = node->Child;
            continue;
        }

        while (node != NULL)
        {
            nextNode = node->Next;

            if (nextNode != NULL)
            {
                node = nextNode;
                break;
            }

            node = node->Parent;
        }
    }

    if (freeEntry != firstNewEntry)
    {
        if (head != NULL)
        {
            ModelHitEntry *tail = head;

            while (tail->next != NULL)
            {
                tail = tail->next;
            }

            tail->next = firstNewEntry;
            firstNewEntry->prev = tail;
        }
        else
        {
            head = firstNewEntry;
        }

        if (freeEntry != NULL)
        {
            ModelHitEntry *lastNewEntry = freeEntry->prev;

            lastNewEntry->next = NULL;
            freeEntry->prev = NULL;
        }

        g_ModelHitFreeList = freeEntry;
    }

    return head;
}
