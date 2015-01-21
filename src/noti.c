/*
 * =====================================================================================
 *
 *       Filename:  noti.c
 *
 *    Description:  Notification functions.
 *
 *        Version:  1.0
 *        Created:  01/21/2015 04:18:41 PM CST
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Leonardo A. Bautista Gomez (leobago@gmail.com),
 *        Company:  Argonne National Laboratory
 *
 * =====================================================================================
 */


#include "fti.h"

/*-------------------------------------------------------------------------*/
/**
    @brief      It gets the metadata to recover the data after a failure.
    @param      fs              Pointer to fill the checkpoint file size.
    @param      mfs             Pointer to fill the maximum file size.
    @param      group           The group in the node.
    @param      level           The level of the ckpt or 0 if tmp.
    @return     integer         FTI_SCES if successfull.

    This function read the metadata file created during checkpointing and
    recover the checkpoint file name, file size and the size of the largest
    file in the group (for padding if ncessary during decoding).

 **/
/*-------------------------------------------------------------------------*/
int FTI_GetNoti() {
    dictionary *ini;
    int res = -1, cnt = 3;
    char mfn[FTI_BUFS], str[FTI_BUFS], *cfn;
    sprintf(mfn,"notifications.fti");
    while (( res != 0) && (cnt > 0))
    {
        FTI_Print("Checking notifications file ...", FTI_DBUG);
        res = access(mfn, R_OK);
        cnt--;
    }
    if (res != 0){
        FTI_Print("Notifications file NOT accessible.", FTI_DBUG);
        return FTI_NSCS;
    }
    ini = iniparser_load(mfn);
    if (ini == NULL)
    {
        FTI_Print("Iniparser failed to parse the metadata file.", FTI_WARN);
        return FTI_NSCS;
    }
    sprintf(str, "CPU:L2DCE");
    cfn = iniparser_getstring(ini, str, NULL);
    snprintf(mfn, FTI_BUFS, "%s", cfn);
    iniparser_freedict(ini);
    sprintf(str, " ***CPU:L2DCE*** => %s", mfn);
    FTI_Print(str, FTI_INFO);
    return FTI_SCES;
}



