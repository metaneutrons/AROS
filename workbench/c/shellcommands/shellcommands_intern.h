/*
    Copyright (C) 1995-2004, The AROS Development Team. All rights reserved.

    Desc: Internal data structures for expansion.library
*/

#ifndef _SHELLCOMMANDS_INTERN_H
#define _SHELLCOMMANDS_INTERN_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dosextens.h>

#include <aros/shcommands.h>

struct ShellCommandsBase {
    struct Library sc_Lib;

    int		sc_Commands;              /* Number of commands */

    /* This is both a segment, and the data for the segment.
     * We will feed in to DOS/AddSegment() the BPTR to 
     * &sc_Command[i].scs_Next as the 'seglist' to add.
     */
    /*
     * AROS segment layout: [ULONG size][BPTR next][code...][name]
     * DOS expects code immediately after the segment header (size+next).
     * Packed is required to prevent padding between scs_Next and scs_Code,
     * since sizeof(ULONG) + sizeof(BPTR) = 12 on 64-bit systems.
     */
    struct ShellCommandSeg {
    	ULONG              scs_Size;      /* Segment size (0 = don't UnLoadSeg) */
    	BPTR               scs_Next;      /* Next segment (always 0) */
    	struct FullJumpVec scs_Code;      /* Jump vector to shell command */
    	CONST_STRPTR       scs_Name;      /* Command name */
    } __attribute__((packed)) *sc_Command;

    /* Bookkeeping */
    BPTR	sc_SegList;

    APTR	sc_DOSBase;
};

extern struct ExecBase *SysBase;
#define DOSBase	(ShellCommandsBase->sc_DOSBase)


#endif /* _SHELLCOMMANDS_INTERN_H */
