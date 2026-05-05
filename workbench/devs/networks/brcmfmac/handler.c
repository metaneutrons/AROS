/*
 * BCM43455 WiFi — SANA-II Device handler (BeginIO/AbortIO)
 */

#include <aros/debug.h>
#include <exec/types.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <devices/sana2.h>
#include <devices/sana2wireless.h>
#include <devices/newstyle.h>
#include <proto/exec.h>
#include <string.h>

#include "brcmfmac.h"

#include LC_LIBDEFS_FILE

static const UWORD supported_commands[] = {
    CMD_READ, CMD_WRITE, CMD_FLUSH,
    S2_DEVICEQUERY, S2_GETSTATIONADDRESS, S2_CONFIGINTERFACE,
    S2_BROADCAST, S2_ONEVENT, S2_ONLINE, S2_OFFLINE,
    S2_GETNETWORKS, S2_GETSIGNALQUALITY,
    NSCMD_DEVICEQUERY,
    0
};

AROS_LH1(void, BeginIO,
    AROS_LHA(struct IOSana2Req *, req, A1),
    LIBBASETYPEPTR, LIBBASE, 5, BrcmfDev)
{
    AROS_LIBFUNC_INIT

    struct BrcmfUnit *unit = (struct BrcmfUnit *)req->ios2_Req.io_Unit;

    req->ios2_Req.io_Error = 0;

    switch (req->ios2_Req.io_Command) {
    case S2_DEVICEQUERY:
    {
        struct Sana2DeviceQuery *query = req->ios2_StatData;
        if (query) {
            query->DevQueryFormat = 0;
            query->DeviceLevel = 0;
            query->AddrFieldSize = 48;
            query->MTU = ETH_MTU;
            query->BPS = 54000000; /* 54 Mbps (802.11g/n) */
            query->HardwareType = S2WireType_IEEE802;
        }
        break;
    }

    case S2_GETSTATIONADDRESS:
        CopyMem(unit->bn_DevAddr, req->ios2_SrcAddr, ETH_ADDRSIZE);
        CopyMem(unit->bn_DevAddr, req->ios2_DstAddr, ETH_ADDRSIZE);
        break;

    case S2_CONFIGINTERFACE:
        if (!(unit->bn_Flags & BNF_CONFIGURED)) {
            CopyMem(req->ios2_SrcAddr, unit->bn_DevAddr, ETH_ADDRSIZE);
            unit->bn_Flags |= BNF_CONFIGURED;
        } else {
            req->ios2_Req.io_Error = S2ERR_BAD_STATE;
            req->ios2_WireError = S2WERR_IS_CONFIGURED;
        }
        break;

    case S2_ONLINE:
        if (!(unit->bn_Flags & BNF_ONLINE)) {
            unit->bn_Flags |= BNF_ONLINE;
        }
        break;

    case S2_OFFLINE:
        if (unit->bn_Flags & BNF_ONLINE) {
            brcmf_cmd_disassoc(unit);
            unit->bn_Flags &= ~BNF_ONLINE;
        }
        break;

    case S2_GETNETWORKS:
        /* Trigger a scan and return results asynchronously */
        if (unit->bn_Flags & BNF_ONLINE) {
            brcmf_cmd_scan(unit);
            /* TODO: Return scan results via event when complete */
            req->ios2_Req.io_Error = 0;
        } else {
            req->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        }
        break;

    case S2_GETSIGNALQUALITY:
    {
        struct TagItem *tags = (struct TagItem *)req->ios2_StatData;
        if (tags && unit->bn_Associated) {
            /* Return current signal quality */
            while (tags->ti_Tag != TAG_DONE) {
                switch (tags->ti_Tag) {
                case S2INFO_Signal:
                    tags->ti_Data = unit->bn_RSSI;
                    break;
                }
                tags++;
            }
        }
        break;
    }

    case CMD_READ:
        if (unit->bn_Flags & BNF_ONLINE) {
            req->ios2_Req.io_Flags &= ~IOF_QUICK;
            ObtainSemaphore(&unit->bn_Lock);
            AddTail((struct List *)&unit->bn_ReadList, (struct Node *)req);
            ReleaseSemaphore(&unit->bn_Lock);
            return;
        } else {
            req->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        }
        break;

    case CMD_WRITE:
    case S2_BROADCAST:
        if (unit->bn_Flags & BNF_ONLINE && unit->bn_Associated) {
            req->ios2_Req.io_Flags &= ~IOF_QUICK;
            ObtainSemaphore(&unit->bn_Lock);
            AddTail((struct List *)&unit->bn_WriteList, (struct Node *)req);
            ReleaseSemaphore(&unit->bn_Lock);
            if (unit->bn_Task)
                Signal(unit->bn_Task, 1 << unit->bn_IntSig);
            return;
        } else {
            req->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        }
        break;

    case CMD_FLUSH:
        ObtainSemaphore(&unit->bn_Lock);
        {
            struct IOSana2Req *r;
            while ((r = (struct IOSana2Req *)RemHead((struct List *)&unit->bn_ReadList))) {
                r->ios2_Req.io_Error = IOERR_ABORTED;
                ReplyMsg((struct Message *)r);
            }
            while ((r = (struct IOSana2Req *)RemHead((struct List *)&unit->bn_WriteList))) {
                r->ios2_Req.io_Error = IOERR_ABORTED;
                ReplyMsg((struct Message *)r);
            }
        }
        ReleaseSemaphore(&unit->bn_Lock);
        break;

    case NSCMD_DEVICEQUERY:
    {
        struct NSDeviceQueryResult *d = (struct NSDeviceQueryResult *)req->ios2_StatData;
        if (d) {
            d->DeviceType = NSDEVTYPE_SANA2;
            d->DeviceSubType = 0;
            d->SupportedCommands = (UWORD *)supported_commands;
        }
        break;
    }

    default:
        req->ios2_Req.io_Error = IOERR_NOCMD;
        break;
    }

    if (!(req->ios2_Req.io_Flags & IOF_QUICK))
        ReplyMsg((struct Message *)req);

    AROS_LIBFUNC_EXIT
}

AROS_LH1(LONG, AbortIO,
    AROS_LHA(struct IOSana2Req *, req, A1),
    LIBBASETYPEPTR, LIBBASE, 6, BrcmfDev)
{
    AROS_LIBFUNC_INIT

    struct BrcmfUnit *unit = (struct BrcmfUnit *)req->ios2_Req.io_Unit;

    ObtainSemaphore(&unit->bn_Lock);
    if (req->ios2_Req.io_Message.mn_Node.ln_Type != NT_REPLYMSG) {
        Remove((struct Node *)req);
        req->ios2_Req.io_Error = IOERR_ABORTED;
        ReplyMsg((struct Message *)req);
    }
    ReleaseSemaphore(&unit->bn_Lock);

    return 0;

    AROS_LIBFUNC_EXIT
}
