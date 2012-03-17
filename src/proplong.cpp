/**************************************************************************
 * File : propLong.c
 * Purpose: propagate the value of long variables (local variables and
 *          registers) along the graph.  Structure the graph in this way.
 * (C) Cristina Cifuentes
 **************************************************************************/
#include <string.h>
#include <memory.h>
#include <cassert>
#include <algorithm>

#include "dcc.h"

/* Returns whether the given icode opcode is within the range of valid
 * high-level conditional jump icodes (iJB..iJG) */
static boolT isJCond (llIcode opcode)
{
    if ((opcode >= iJB) && (opcode <= iJG))
        return true;
    return false;
}


/* Returns whether the conditions for a 2-3 long variable are satisfied */
static bool isLong23 (BB * pbb, iICODE &off, int *arc)
{
    BB * t, * e, * obb2;

    if (pbb->nodeType != TWO_BRANCH)
        return false;
    t = pbb->edges[THEN].BBptr;
    e = pbb->edges[ELSE].BBptr;

    /* Check along the THEN path */
    if ((t->size() == 1) && (t->nodeType == TWO_BRANCH) && (t->inEdges.size() == 1))
    {
        obb2 = t->edges[THEN].BBptr;
        if ((obb2->size() == 2) && (obb2->nodeType == TWO_BRANCH) && (obb2->front().ll()->getOpcode() == iCMP))
        {
            off = obb2->begin();//std::distance(iter,obb2->begin2());
            *arc = THEN;
            return true;
        }
    }

    /* Check along the ELSE path  */
    else if ((e->size() == 1) && (e->nodeType == TWO_BRANCH) && (e->inEdges.size() == 1))
    {
        obb2 = e->edges[THEN].BBptr;
        if ((obb2->size() == 2) && (obb2->nodeType == TWO_BRANCH) &&  (obb2->front().ll()->getOpcode() == iCMP))
        {
            off = obb2->begin();//std::distance(iter,obb2->begin2());//obb2->front().loc_ip - i;
            *arc = ELSE;
            return true;
        }
    }
    return false;
}


/* Returns whether the conditions for a 2-2 long variable are satisfied */
static boolT isLong22 (iICODE pIcode, iICODE pEnd, iICODE &off)
{
    iICODE initial_icode=pIcode;
    if(distance(pIcode,pEnd)<4)
        return false;
    // preincrement because pIcode is not checked here
    iICODE icodes[] = { ++pIcode,++pIcode,++pIcode };
    if (   icodes[1]->ll()->match(iCMP) &&
           (isJCond ((llIcode)icodes[0]->ll()->getOpcode())) &&
           (isJCond ((llIcode)icodes[2]->ll()->getOpcode())))
    {
        off = initial_icode;
        advance(off,2);
        return true;
    }
    return false;
}

/** Creates a long conditional <=, >=, <, or > at (pIcode+1).
 * Removes excess nodes from the graph by flagging them, and updates
 * the new edges for the remaining nodes.
 * @return number of ICODEs to skip

*/
static int longJCond23 (COND_EXPR *rhs, COND_EXPR *lhs, iICODE pIcode, int arc, iICODE atOffset)
{
    BB * pbb, * obb1, * obb2, * tbb;
    int skipped_insn=0;
    if (arc == THEN)
    {
        /* Find intermediate basic blocks and target block */
        pbb = pIcode->getParent();
        obb1 = pbb->edges[THEN].BBptr;
        obb2 = obb1->edges[THEN].BBptr;
        tbb = obb2->edges[THEN].BBptr;

        /* Modify out edge of header basic block */
        pbb->edges[THEN].BBptr = tbb;

        /* Modify in edges of target basic block */
        auto newlast=std::remove_if(tbb->inEdges.begin(),tbb->inEdges.end(),
                                    [obb1,obb2](BB *b) -> bool {
                                    return (b==obb1) || (b==obb2); });
        tbb->inEdges.erase(newlast,tbb->inEdges.end());
        tbb->inEdges.push_back(pbb); /* looses 2 arcs, gains 1 arc */

        /* Modify in edges of the ELSE basic block */
        tbb = pbb->edges[ELSE].BBptr;
        auto iter=std::find(tbb->inEdges.begin(),tbb->inEdges.end(),obb2);
        assert(iter!=tbb->inEdges.end());
        tbb->inEdges.erase(iter); /* looses 1 arc */
        /* Update icode index */
        skipped_insn = 5;
    }
    else  /* ELSE arc */
    {
        /* Find intermediate basic blocks and target block */
        pbb = pIcode->getParent();
        obb1 = pbb->edges[ELSE].BBptr;
        obb2 = obb1->edges[THEN].BBptr;
        tbb = obb2->edges[THEN].BBptr;

        /* Modify in edges of target basic block */
        auto iter=std::find(tbb->inEdges.begin(),tbb->inEdges.end(),obb2);
        assert(iter!=tbb->inEdges.end());
        tbb->inEdges.erase(iter); /* looses 1 arc */

        /* Modify in edges of the ELSE basic block */
        tbb = obb2->edges[ELSE].BBptr;
        auto newlast=std::remove_if(tbb->inEdges.begin(),tbb->inEdges.end(),
                                    [obb1,obb2](BB *b) -> bool { return (b==obb1) || (b==obb2); });
        tbb->inEdges.erase(newlast,tbb->inEdges.end());
        tbb->inEdges.push_back(pbb); /* looses 2 arcs, gains 1 arc */

        /* Modify out edge of header basic block */
        pbb->edges[ELSE].BBptr = tbb;

        /* Update icode index */
        skipped_insn = 2;
    }
    iICODE atOffset1(atOffset),next1(++iICODE(pIcode));
    advance(atOffset1,1);
    /* Create new HLI_JCOND and condition */
    lhs = COND_EXPR::boolOp (lhs, rhs, condOpJCond[atOffset1->ll()->getOpcode()-iJB]);
    next1->setJCond(lhs);
    next1->copyDU(*pIcode, eUSE, eUSE);
    next1->du.use |= atOffset->du.use;

    /* Update statistics */
    obb1->flg |= INVALID_BB;
    obb2->flg |= INVALID_BB;
    stats.numBBaft -= 2;

    pIcode->invalidate();
    obb1->front().invalidate();
    // invalidate 2 first instructions of BB 2
    iICODE ibb2 = obb2->begin();
    (ibb2++)->invalidate();
    (ibb2++)->invalidate();
    return skipped_insn;
}


/** Creates a long conditional equality or inequality at (pIcode+1).
 * Removes excess nodes from the graph by flagging them, and updates
 * the new edges for the remaining nodes.
 * @return number of ICODE's to skip
*/
static int longJCond22 (COND_EXPR *rhs, COND_EXPR *lhs, iICODE pIcode,iICODE pEnd)
{

    BB * pbb, * obb1, * tbb;
    if(distance(pIcode,pEnd)<4)
        return false;
    // preincrement because pIcode is not checked here
    iICODE icodes[] = { pIcode++,pIcode++,pIcode++,pIcode++ };

    /* Form conditional expression */
    lhs = COND_EXPR::boolOp (lhs, rhs, condOpJCond[icodes[3]->ll()->getOpcode() - iJB]);
    icodes[1]->setJCond(lhs);
    icodes[1]->copyDU (*icodes[0], eUSE, eUSE);
    icodes[1]->du.use |= icodes[2]->du.use;

    /* Adjust outEdges[0] to the new target basic block */
    pbb = icodes[0]->getParent();
    if (pbb->back().loc_ip == icodes[1]->loc_ip)
    {
        /* Find intermediate and target basic blocks */
        obb1 = pbb->edges[THEN].BBptr;
        tbb = obb1->edges[THEN].BBptr;

        /* Modify THEN out edge of header basic block */
        pbb->edges[THEN].BBptr = tbb;

        /* Modify in edges of target basic block */
        auto iter=std::find(tbb->inEdges.begin(),tbb->inEdges.end(),obb1);
        assert(iter!=tbb->inEdges.end());
        tbb->inEdges.erase(iter);

        if (icodes[3]->ll()->getOpcode() != iJE)
            tbb->inEdges.push_back(pbb); /* iJNE => replace arc */

        /* Modify ELSE out edge of header basic block */
        tbb = obb1->edges[ELSE].BBptr;
        pbb->edges[ELSE].BBptr = tbb;

        iter=std::find(tbb->inEdges.begin(),tbb->inEdges.end(),obb1);
        assert(iter!=tbb->inEdges.end());
        tbb->inEdges.erase(iter);
        if (icodes[3]->ll()->getOpcode() == iJE)	/* replace */
            tbb->inEdges.push_back(pbb);

        /* Update statistics */
        obb1->flg |= INVALID_BB;
        stats.numBBaft--;
    }

    icodes[0]->invalidate();
    icodes[2]->invalidate();
    icodes[3]->invalidate();
    return 4;
}

/* Propagates TYPE_LONG_(UN)SIGN icode information to the current pIcode
 * Pointer.
 * Arguments: i     : index into the local identifier table
 *            pLocId: ptr to the long local identifier
 *            pProc : ptr to current procedure's record.        */
void Function::propLongStk (int i, const ID &pLocId)
{
    int arc;
    Assignment asgn;
    //COND_EXPR *lhs, *rhs;     /* Pointers to left and right hand expression */
    iICODE next1, pEnd;
    iICODE l23;
    /* Check all icodes for offHi:offLo */
    pEnd = Icode.end();
    int stat_size=Icode.size();
//    for (idx = 0; idx < (Icode.size() - 1); idx++)
    for(auto pIcode = Icode.begin(); ;++pIcode)
    {
        assert(Icode.size()==stat_size);
        next1 = ++iICODE(pIcode);
        if(next1==pEnd)
            break;
        if ((pIcode->type == HIGH_LEVEL) || ( not pIcode->valid() ))
            continue;
        if (pIcode->ll()->getOpcode() == next1->ll()->getOpcode())
        {
            if (checkLongEq (pLocId.id.longStkId, pIcode, i, this, asgn, next1) == true)
            {
                switch (pIcode->ll()->getOpcode())
                {
                case iMOV:
                    pIcode->setAsgn(asgn.lhs, asgn.rhs);
                    next1->invalidate();
                    break;

                case iAND: case iOR: case iXOR:
                    switch (pIcode->ll()->getOpcode())
                    {
                    case iAND: 	asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, AND); break;
                    case iOR: 	asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, OR); break;
                    case iXOR: 	asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, XOR); break;
                    }
                    pIcode->setAsgn(asgn.lhs, asgn.rhs);
                    next1->invalidate();
                    break;

                case iPUSH:
                    pIcode->setUnary( HLI_PUSH, asgn.lhs);
                    next1->invalidate();
                    break;
                default:
                    printf("Wild ass checkLongEq success on opcode %d\n",pIcode->ll()->getOpcode());
                } /*eos*/
            }
        }
        //TODO: Simplify this!
        /* Check long conditional (i.e. 2 CMPs and 3 branches */
        else if ((pIcode->ll()->getOpcode() == iCMP) && (isLong23 (pIcode->getParent(), l23, &arc)))
        {
            if ( checkLongEq (pLocId.id.longStkId, pIcode, i, this, asgn, l23) )
            {
                advance(pIcode,longJCond23 (asgn.rhs, asgn.lhs, pIcode, arc, l23));
            }
        }

        /* Check for long conditional equality or inequality.  This requires
                 * 2 CMPs and 2 branches */
        else if ((pIcode->ll()->getOpcode() == iCMP) && isLong22 (pIcode, pEnd, l23))
        {
            if ( checkLongEq (pLocId.id.longStkId, pIcode, i, this,asgn, l23) )
            {
                advance(pIcode,longJCond22 (asgn.rhs, asgn.lhs, pIcode,pEnd));
            }
        }
    }
}
int Function::findBackwarLongDefs(int loc_ident_idx, const ID &pLocId, iICODE beg)
{
    Assignment asgn;
    LLOperand * pmH,* pmL;
    iICODE pIcode;
    riICODE rev(beg);
    bool forced_finish=false;
    for (; not forced_finish and rev!=Icode.rend();rev++) //idx = pLocId_idx - 1; idx > 0 ; idx--
    {
        pIcode = (++riICODE(rev)).base();//forward iterator from rev
        iICODE next1((++iICODE(pIcode))); // next instruction
        ICODE &icode(*pIcode);


        if ((icode.type == HIGH_LEVEL) || ( not icode.valid() ))
            continue;
        if (icode.ll()->getOpcode() != next1->ll()->getOpcode())
            continue;

        switch (icode.ll()->getOpcode())
        {
        case iMOV:
            pmH = &icode.ll()->dst;
            pmL = &next1->ll()->dst;
            if ((pLocId.id.longId.h == pmH->regi) && (pLocId.id.longId.l == pmL->regi))
            {
                localId.id_arr[loc_ident_idx].idx.push_back(pIcode);//idx-1//insert
                icode.setRegDU( pmL->regi, eDEF);
                asgn.lhs = COND_EXPR::idLongIdx (loc_ident_idx);
                asgn.rhs = COND_EXPR::idLong (&this->localId, SRC, pIcode, HIGH_FIRST, pIcode, eUSE, next1);
                icode.setAsgn(asgn.lhs, asgn.rhs);
                next1->invalidate();
                forced_finish=true; /* to exit the loop */
            }
            break;

        case iPOP:
            pmH = &next1->ll()->dst;
            pmL = &icode.ll()->dst;
            if ((pLocId.id.longId.h == pmH->regi) && (pLocId.id.longId.l == pmL->regi))
            {
                asgn.lhs = COND_EXPR::idLongIdx (loc_ident_idx);
                icode.setRegDU( pmH->regi, eDEF);
                icode.setUnary(HLI_POP, asgn.lhs);
                next1->invalidate();
                asgn.lhs=0;
                forced_finish=true;        /* to exit the loop */
            }
            break;

            //                /**** others missing ***/

        case iAND: case iOR: case iXOR:
            pmL = &icode.ll()->dst;
            pmH = &next1->ll()->dst;
            if ((pLocId.id.longId.h == pmH->regi) && (pLocId.id.longId.l == pmL->regi))
            {
                asgn.lhs = COND_EXPR::idLongIdx (loc_ident_idx);
                asgn.rhs = COND_EXPR::idLong (&this->localId, SRC, pIcode, LOW_FIRST, pIcode/*idx*/, eUSE, next1);
                icode.setRegDU( pmH->regi, USE_DEF);
                switch (icode.ll()->getOpcode())
                {
                case iAND: asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, AND);
                    break;
                case iOR:
                    asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, OR);
                    break;
                case iXOR: asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, XOR);
                    break;
                } /* eos */
                icode.setAsgn(asgn.lhs, asgn.rhs);
                next1->invalidate();
                forced_finish=true;        /* to exit the loop */
            }
            break;
        } /* eos */
    }
    return rev!=Icode.rend();
}
int Function::findForwardLongUses(int loc_ident_idx, const ID &pLocId, iICODE beg)
{
    bool forced_finish=false;
    auto pEnd=Icode.end();
    iICODE long_loc;
    Assignment asgn;
    for (auto pIcode=beg; not forced_finish; ++pIcode)
    {
        iICODE next1(++iICODE(pIcode));
        if(next1==pEnd)
            break;
        LLOperand * pmH,* pmL;            /* Pointers to dst LOW_LEVEL icodes */
        int arc;

        if ((pIcode->type == HIGH_LEVEL) || ( not pIcode->valid() ))
            continue;

        if (pIcode->ll()->getOpcode() == next1->ll()->getOpcode())
            switch (pIcode->ll()->getOpcode())
            {
            case iMOV:
                if ((pLocId.id.longId.h == pIcode->ll()->src.regi) &&
                        (pLocId.id.longId.l == next1->ll()->src.regi))
                {
                    pIcode->setRegDU( next1->ll()->src.regi, eUSE);

                    asgn.rhs = COND_EXPR::idLongIdx (loc_ident_idx);
                    asgn.lhs = COND_EXPR::idLong (&this->localId, DST, pIcode,HIGH_FIRST, pIcode, eDEF, next1);

                    pIcode->setAsgn(asgn.lhs, asgn.rhs);
                    next1->invalidate();
                    forced_finish =true; /* to exit the loop */
                }
                break;

            case iPUSH:
                if ((pLocId.id.longId.h == pIcode->ll()->src.regi) &&
                        (pLocId.id.longId.l == next1->ll()->src.regi))
                {
                    asgn.rhs = COND_EXPR::idLongIdx (loc_ident_idx);
                    pIcode->setRegDU( next1->ll()->src.regi, eUSE);
                    pIcode->setUnary(HLI_PUSH, asgn.rhs);
                    next1->invalidate();
                }
                forced_finish =true; /* to exit the loop */
                break;

                /*** others missing ****/

            case iAND: case iOR: case iXOR:
                pmL = &pIcode->ll()->dst;
                pmH = &next1->ll()->dst;
                if ((pLocId.id.longId.h == pmH->regi) &&
                        (pLocId.id.longId.l == pmL->regi))
                {
                    asgn.lhs = COND_EXPR::idLongIdx (loc_ident_idx);
                    pIcode->setRegDU( pmH->regi, USE_DEF);
                    asgn.rhs = COND_EXPR::idLong (&this->localId, SRC, pIcode,
                                                  LOW_FIRST, pIcode, eUSE, next1);
                    switch (pIcode->ll()->getOpcode()) {
                    case iAND: asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, AND);
                        break;
                    case iOR:  asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, OR);
                        break;
                    case iXOR: asgn.rhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, XOR);
                        break;
                    }
                    pIcode->setAsgn(asgn.lhs, asgn.rhs);
                    next1->invalidate();
                    // ftw loop restart ????
                    //idx = 0;
                    // maybe this should end the loop instead
                    forced_finish =true; /* to exit the loop */
                }
                break;
            } /* eos */

        /* Check long conditional (i.e. 2 CMPs and 3 branches */
        else if ((pIcode->ll()->getOpcode() == iCMP) && (isLong23 (pIcode->getParent(), long_loc, &arc)))
        {
            if (checkLongRegEq (pLocId.id.longId, pIcode, loc_ident_idx, this, asgn, long_loc))
            {
                                // reduce the advance by 1 here (loop increases) ?
                advance(pIcode,longJCond23 (asgn.rhs, asgn.lhs, pIcode, arc, long_loc));
            }
        }

        /* Check for long conditional equality or inequality.  This requires
             * 2 CMPs and 2 branches */
        else if (pIcode->ll()->match(iCMP) && (isLong22 (pIcode, pEnd, long_loc)))
        {
            if (checkLongRegEq (pLocId.id.longId, pIcode, loc_ident_idx, this, asgn, long_loc) )
            {
                advance(pIcode,longJCond22 (asgn.rhs, asgn.lhs, pIcode,pEnd) - 1);
            }
        }

        /* Check for OR regH, regL
             *			 JX lab
             *		=> HLI_JCOND (regH:regL X 0) lab
             * This is better code than HLI_JCOND (HI(regH:regL) | LO(regH:regL)) */
        else if (pIcode->ll()->match(iOR) && (next1 != pEnd) && (isJCond ((llIcode)next1->ll()->getOpcode())))
        {
            if (pLocId.id.longId.srcDstRegMatch(pIcode,pIcode))
            {
                asgn.lhs = COND_EXPR::idLongIdx (loc_ident_idx);
                asgn.rhs = COND_EXPR::idKte (0, 4);	/* long 0 */
                asgn.lhs = COND_EXPR::boolOp (asgn.lhs, asgn.rhs, condOpJCond[next1->ll()->getOpcode() - iJB]);
                next1->setJCond(asgn.lhs);
                next1->copyDU(*pIcode, eUSE, eUSE);
                pIcode->invalidate();
            }
        }
    } /* end for */
}

/** Finds the definition of the long register pointed to by pLocId, and
 * transforms that instruction into a HIGH_LEVEL icode instruction.
 * @arg i     index into the local identifier table
 * @arg pLocId ptr to the long local identifier
 *
 */
void Function::propLongReg (int loc_ident_idx, const ID &pLocId)
{
    /* Process all definitions/uses of long registers at an icode position */
    // WARNING: this loop modifies the iterated-over container.
    size_t initial_size=pLocId.idx.size();
    for (int j = 0; j < pLocId.idx.size(); j++)
    {
        auto idx_iter=pLocId.idx.begin();
        std::advance(idx_iter,j);
        /* Check backwards for a definition of this long register */
        if (findBackwarLongDefs(loc_ident_idx,pLocId,*idx_iter))
        {
            //assert(initial_size==pLocId.idx.size());
            continue;
        }
        /* If no definition backwards, check forward for a use of this long reg */
        findForwardLongUses(loc_ident_idx,pLocId,*idx_iter);
        //assert(initial_size==pLocId.idx.size());
    } /* end for */
}


/* Propagates the long global address across all LOW_LEVEL icodes.
 * Transforms some LOW_LEVEL icodes into HIGH_LEVEL     */
void Function::propLongGlb (int i, const ID &pLocId)
{
    printf("WARN: Function::propLongGlb not implemented");
}


/* Propagated identifier information, thus converting some LOW_LEVEL icodes
 * into HIGH_LEVEL icodes.  */
void Function::propLong()
{
    int i;
    /* Pointer to current local identifier */

    for (i = 0; i < localId.csym(); i++)
    {
        const ID &pLocId(localId.id_arr[i]);
        if ((pLocId.type==TYPE_LONG_SIGN) || (pLocId.type==TYPE_LONG_UNSIGN))
        {
            switch (pLocId.loc)
            {
            case STK_FRAME:
                propLongStk (i, pLocId);
                break;
            case REG_FRAME:
                propLongReg (i, pLocId);
                break;
            case GLB_FRAME:
                propLongGlb (i, pLocId);
                break;
            }
        }
    }
}

