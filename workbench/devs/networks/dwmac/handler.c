/*
 * DesignWare MAC — SANA-II handler (BeginIO/AbortIO)
 * Same pattern as genet handler.
 */

#include <aros/debug.h>
#include <exec/types.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <devices/sana2.h>
#include <devices/newstyle.h>
#include <proto/exec.h>
#include <string.h>

#include "dwmac.h"

#include LC_LIBDEFS_FILE

static const UWORD supported_commands[] = {
    CMD_READ, CMD_WRITE, CMD_FLUSH,
    S2_DEVICEQUERY, S2_GETSTATIONADDRESS, S2_CONFIGINTERFACE,
    S2_BROADCAST, S2_ONEVENT, S2_ONLINE, S2_OFFLINE,
    S2_GETGLOBALSTATS, NSCMD_DEVICEQUERY, 0
};

AROS_LH1(void, BeginIO,
    AROS_LHA(struct IOSana2Req *, req, A1),
    LIBBASETYPEPTR, LIBBASE, 5, DWMACDev)
{
    AROS_LIBFUNC_INIT

    struct DWMACUnit *unit = (struct DWMACUnit *)req->ios2_Req.io_Unit;
    req->ios2_Req.io_Error = 0;

    switch (req->ios2_Req.io_Command) {
    case S2_DEVICEQUERY:
    {
        struct Sana2DeviceQuery *q = req->ios2_StatData;
        if (q) {
            q->DevQueryFormat = 0;
            q->DeviceLevel = 0;
            q->AddrFieldSize = 48;
            q->MTU = ETH_MTU;
            q->BPS = 1000000000;
            q->HardwareType = S2WireType_Ethernet;
        }
        break;
    }
    case S2_GETSTATIONADDRESS:
        CopyMem(unit->du_DevAddr, req->ios2_SrcAddr, ETH_ADDRSIZE);
        CopyMem(unit->du_OrgAddr, req->ios2_DstAddr, ETH_ADDRSIZE);
        break;
    case S2_CONFIGINTERFACE:
        if (!(unit->du_Flags & DUF_CONFIGURED)) {
            CopyMem(req->ios2_SrcAddr, unit->du_DevAddr, ETH_ADDRSIZE);
            unit->du_Flags |= DUF_CONFIGURED;
            dwmac_hw_set_mac(unit, unit->du_DevAddr);
        } else {
            req->ios2_Req.io_Error = S2ERR_BAD_STATE;
            req->ios2_WireError = S2WERR_IS_CONFIGURED;
        }
        break;
    case S2_ONLINE:
        if (!(unit->du_Flags & DUF_ONLINE) && unit->du_RegBase) {
            dwmac_hw_init(unit);
            unit->du_Flags |= DUF_ONLINE;
        }
        break;
    case S2_OFFLINE:
        if (unit->du_Flags & DUF_ONLINE) {
            dwmac_hw_stop(unit);
            unit->du_Flags &= ~DUF_ONLINE;
        }
        break;
    case CMD_READ:
        if (unit->du_Flags & DUF_ONLINE) {
            req->ios2_Req.io_Flags &= ~IOF_QUICK;
            ObtainSemaphore(&unit->du_Lock);
            AddTail((struct List *)&unit->du_ReadList, (struct Node *)req);
            ReleaseSemaphore(&unit->du_Lock);
            if (unit->du_Task)
                Signal(unit->du_Task, 1 << unit->du_IntSig);
            return;
        }
        req->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        break;
    case CMD_WRITE:
    case S2_BROADCAST:
        if (unit->du_Flags & DUF_ONLINE) {
            req->ios2_Req.io_Flags &= ~IOF_QUICK;
            ObtainSemaphore(&unit->du_Lock);
            AddTail((struct List *)&unit->du_WriteList, (struct Node *)req);
            ReleaseSemaphore(&unit->du_Lock);
            if (unit->du_Task)
                Signal(unit->du_Task, 1 << unit->du_IntSig);
            return;
        }
        req->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        break;
    case CMD_FLUSH:
        ObtainSemaphore(&unit->du_Lock);
        {
            struct IOSana2Req *r;
            while ((r = (struct IOSana2Req *)RemHead((struct List *)&unit->du_ReadList))) {
                r->ios2_Req.io_Error = IOERR_ABORTED;
                ReplyMsg((struct Message *)r);
            }
        }
        ReleaseSemaphore(&unit->du_Lock);
        break;
    case S2_GETGLOBALSTATS:
        CopyMem(&unit->du_Stats, req->ios2_StatData, sizeof(struct Sana2DeviceStats));
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
    LIBBASETYPEPTR, LIBBASE, 6, DWMACDev)
{
    AROS_LIBFUNC_INIT
    struct DWMACUnit *unit = (struct DWMACUnit *)req->ios2_Req.io_Unit;
    ObtainSemaphore(&unit->du_Lock);
    if (req->ios2_Req.io_Message.mn_Node.ln_Type != NT_REPLYMSG) {
        Remove((struct Node *)req);
        req->ios2_Req.io_Error = IOERR_ABORTED;
        ReplyMsg((struct Message *)req);
    }
    ReleaseSemaphore(&unit->du_Lock);
    return 0;
    AROS_LIBFUNC_EXIT
}
