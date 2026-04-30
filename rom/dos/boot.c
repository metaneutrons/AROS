/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Implements AROS's generic/amiga-like boot sequence.
*/

#include <aros/debug.h>
#include <exec/alerts.h>
#include <exec/libraries.h>
#include <exec/devices.h>
#include <exec/execbase.h>
#include <aros/libcall.h>
#include <aros/asmcall.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/cliinit.h>
#include <intuition/screens.h>
#include <dos/stdio.h>
#include <utility/tagitem.h>
#include <libraries/expansionbase.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

#include "dos_intern.h"
#include "../dosboot/bootflags.h"

extern char *generate_banner(void);

#ifdef __mc68000
/*
 * Load DEVS:system-configuration only on m68k.
 * Setup pre-2.0 boot disk colors and mouse cursors (for example)
 */
#define USE_SYSTEM_CONFIGURATION

#endif

#ifdef USE_SYSTEM_CONFIGURATION

#include <proto/intuition.h>

static void load_system_configuration(struct DosLibrary *DOSBase)
{
    BPTR fh;
    ULONG len;
    struct Preferences prefs;
    struct Library *IntuitionBase;

    fh = Open("DEVS:system-configuration", MODE_OLDFILE);
    if (!fh)
        return;
    len = Read(fh, &prefs, sizeof prefs);
    Close(fh);
    if (len != sizeof prefs)
        return;
    IntuitionBase = TaggedOpenLibrary(TAGGEDOPEN_INTUITION);
    if (IntuitionBase)
        SetPrefs(&prefs, len, FALSE);
    CloseLibrary(IntuitionBase);
}

#else

#define load_system_configuration(DOSBase) do { } while (0)

#endif

extern void BCPL_cliInit(void);

void __dos_Boot(struct DosLibrary *DOSBase, ULONG BootFlags, UBYTE Flags)
{
#if defined(HOST_OS_darwin) && defined(__aarch64__)
    /* Load Cocoa display driver directly - ExAll/MatchFirst has a double-free bug
     * that prevents using AROSMonDrvs for directory scanning.
     * Use CreateNewProcTags to run it asynchronously. */
    bug("[DOS] darwin-aarch64: loading Cocoa display driver\n");
    {
        BPTR seg = LoadSeg("DEVS:Monitors/CocoaGfx");
        if (seg) {
            struct Process *proc = CreateNewProcTags(
                NP_Seglist, (IPTR)seg,
                NP_FreeSeglist, TRUE,
                NP_Name, (IPTR)"Cocoa Display",
                NP_Priority, 0,
                TAG_DONE);
            bug("[DOS] Cocoa display process=%p\n", proc);
            /* Give it time to initialize */
            Delay(50);
            bug("[DOS] Opening screen...\n");
            {
                struct Library *IntuitionBase = OpenLibrary("intuition.library", 0);
                if (IntuitionBase) {
                    struct Screen *scr;
                    struct TagItem scrTags[] = {
                        { SA_Width, 640 },
                        { SA_Height, 480 },
                        { SA_Depth, 24 },
                        { SA_Title, (IPTR)"AROS on macOS" },
                        { SA_ShowTitle, TRUE },
                        { TAG_DONE, 0 }
                    };
                    scr = (struct Screen *)AROS_LC2(struct Screen *, OpenScreenTagList,
                        AROS_LCA(struct NewScreen *, NULL, A0),
                        AROS_LCA(struct TagItem *, scrTags, A1),
                        struct Library *, IntuitionBase, 102, Intuition);
                    bug("[DOS] OpenScreenTagList=%p\n", scr);
                    /* Don't close - keep screen open */
                    CloseLibrary(IntuitionBase);
                }
            }
        } else {
            bug("[DOS] Cocoa display driver not found\n");
        }
    }
    bug("[DOS] Executing S:Startup-Sequence\n");
    {
        BPTR fh = Open("S:Startup-Sequence", MODE_OLDFILE);
        if (fh) {
            Execute("", fh, BNULL);
            Close(fh);
        } else {
            bug("[DOS] No S:Startup-Sequence\n");
        }
    }
    bug("[DOS] Boot complete\n");
    Wait(0);
    return;
#endif
    BPTR cis = BNULL;

    /*  We have been created as a process by DOS, we should now
        try and boot the system. */

    D(
        bug("[DOS] %s: ** starting generic boot sequence\n", __func__);
        bug("[DOS] %s: BootFlags 0x%08X Flags 0x%02X\n", __func__, BootFlags, Flags);
        bug("[DOS] %s: DOSBase @ 0x%p\n", __func__, DOSBase);
      )

    /* m68000 uses this to get the default colors and
     * cursors for Workbench
     */
    load_system_configuration(DOSBase);

    D(bug("[DOS] %s: system config loaded\n", __func__);)

    /*
     * If needed, run the display drivers loader.
     * In fact the system must have at least one resident driver,
     * which will be used for bootmenu etc. However, it we somehow happen
     * not to have it, this will be our last chance.
     */
    if ((BootFlags & (BF_NO_DISPLAY_DRIVERS | BF_NO_COMPOSITION)) != (BF_NO_DISPLAY_DRIVERS | BF_NO_COMPOSITION))
    {
        /* Check that it exists first... */
        BPTR seg;

        D(bug("[DOS] %s: initialising displays\n", __func__);)

        if ((seg = LoadSeg("C:AROSMonDrvs")) != BNULL)
        {
            STRPTR args = "";
            BPTR oldin, oldout;

            /*
             * Argument strings MUST contain terminating LF because of ReadItem() bugs.
             * Their absence causes ReadArgs() crash.
             */
            if (BootFlags & BF_NO_COMPOSITION)
                args = "NOCOMPOSITION\n";
            else if (BootFlags & BF_NO_DISPLAY_DRIVERS)
                args = "ONLYCOMPOSITION\n";

            D(bug("[DOS] %s: Running AROSMonDrvs %s\n", __func__, args);)

            /* RunCommand needs a valid Input() handle
             * for passing in its arguments.
             */
            oldin = SelectInput(Open("NIL:", MODE_OLDFILE));
            oldout= SelectOutput(Open("NIL:", MODE_NEWFILE));
            RunCommand(seg, AROS_STACKSIZE, args, strlen(args));
            SelectInput(oldin);
            SelectOutput(oldout);

            /* We don't care about the return code */
            UnLoadSeg(seg);
        }
    }

    D(bug("[DOS] %s: preparing console\n", __func__);)

    /*
     * Open the Workbench screen before the boot console so the
     * CON: handler opens its window on the WB screen. This is
     * essential on single-framebuffer displays (e.g. Raspberry Pi)
     * and harmless on multi-screen systems.
     */
    {
        struct Library *IntuitionBase = TaggedOpenLibrary(TAGGEDOPEN_INTUITION);
        if (IntuitionBase) {
            AROS_LC0(IPTR, OpenWorkBench,
                     struct Library *, IntuitionBase, 35, Intuition);
            CloseLibrary(IntuitionBase);
        }
    }

    if (BootFlags & BF_EMERGENCY_CONSOLE) {
        D(bug("[DOS] %s:     (emergency console)\n", __func__);)
        BootFlags |= BF_NO_STARTUP_SEQUENCE;
        cis = Open("ECON:", MODE_OLDFILE);
    }

    if (cis == BNULL)
        cis = Open("CON:////AROS/AUTO/CLOSE/SMART/BOOT", MODE_OLDFILE);

    if (cis) {
        BPTR cos = OpenFromLock(DupLockFromFH(cis));
        BYTE *C = generate_banner();

        D(bug("[DOS] %s:  handle @ 0x%p (0x%p)\n", __func__, cis, cos);)

        if (cos) {
            BPTR cas = BNULL;

            if (!(BootFlags & BF_NO_STARTUP_SEQUENCE))
                cas = Open("S:Startup-Sequence", MODE_OLDFILE);

            /* Inject the banner */
            if (Flags & EBF_SILENTSTART) {
                if (SetVBuf(cos, NULL, BUF_FULL, sizeof(C)) == 0) {
                    FPuts(cos, C);
                    SetVBuf(cos, NULL, BUF_LINE, -1);
                }
            } else {
                FPuts(cos, C);
            }

            D(bug("[DOS] %s: initialising CLI\n", __func__);)

            if (SystemTags(NULL,
                           NP_Name, "Initial CLI",
                           SYS_Background, FALSE,
                           SYS_Asynch, FALSE,
                           SYS_Input, cis,
                           SYS_Output, cos,
                           SYS_ScriptInput, cas,
                           TAG_END) == -1) {
                D(bug("[DOS] %s:  .. failed!\n", __func__);)
                Alert(AT_DeadEnd | AN_BootStrap);
            }

            Close(cis);
#if (1)
            /* Do not flush cos (show banner) if we got this far, we don't want to
             * see shell window quickly opening and then immediately closing at
             * the end of startup-sequence.
             *
             * There has to be less hacky way..
             */
            struct FileHandle *fh = ((struct FileHandle*)BADDR(cos));
            fh->fh_Flags &= ~0x80000000;
#endif
            Close(cos);
            /* NOTE: 'cas' will already have been closed by the Shell */
        }
        FreeVec(C);
    } else {
        D(bug("[DOS] %s:  .. failed!\n", __func__);)
        Alert(AN_NoWindow);
    }
}
