/* Matrix traversal before the LOD cache, retained for differential checks. */
static void referenceUpdateMatrices(ModelRenderData *arg0, Model *model)
{
    ModelNode *node = model->obj->RootNode;

    while (node)
    {
        u32 type = node->Opcode & 0xff;

        switch (type)
        {
            case MODELNODE_OPCODE_HEADER:
                process_01_group_heading(arg0, model, node);
                break;

            case MODELNODE_OPCODE_GROUP:
                process_02_position(arg0, model, node);
                break;

            case MODELNODE_OPCODE_OP03:
                process_03_unknown(arg0, model, node);
                break;

            case MODELNODE_OPCODE_GROUPSIMPLE:
                process_15_subposition(arg0, model, node);
                break;

            case MODELNODE_OPCODE_LOD:
                modelUpdateDistanceRelations(model, node);
                break;

            case MODELNODE_OPCODE_BSP:
                modelUpdateReorderRelations(model, node);
                break;

            case MODELNODE_OPCODE_OP07:
                //process_07_unknown(model, node);
                break;

            case MODELNODE_OPCODE_SWITCH:
                modelApplyToggleRelations(model, node);
                break;

            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;

            case MODELNODE_OPCODE_DLCOLLISION:
            default:
                break;
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}
