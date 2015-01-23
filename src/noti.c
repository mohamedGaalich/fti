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
    struct stat st;
    int size = 0, res;
    char str[FTI_BUFS];
    if (access(FTI_Noti.filePath, R_OK) != 0)
    {
        FTI_Print("Notifications file NOT accessible.", FTI_DBUG);
        return FTI_NSCS;
    }
    stat(FTI_Noti.filePath, &st);
    size = st.st_size;
    if (size > FTI_Noti.size)
    {
        FILE *fh = fopen(FTI_Noti.filePath, "r");
        if (fh == NULL)
        {
            FTI_Print("Notification file can NOT be open.", FTI_DBUG);
            return FTI_NSCS;
        }
        fseek(fh, FTI_Noti.position, SEEK_SET);
        while (!feof(fh))
        {
            if (fgets(str, FTI_BUFS, fh) != NULL)
            {
                char *noti, *tofree, *stamp, *code, *msg;
                noti = strdup(str);
                tofree = noti;
                stamp = strsep(&noti, "|");
                code = strsep(&noti, "|");
                msg = strsep(&noti, "|");
                if ((stamp != NULL) && (code != NULL) && (msg != NULL))
                {
                    if (msg[strlen(msg)-1] == '\n')
                    {
                        msg[strlen(msg)-1] = ' ';
                    }
                    sprintf(str, "[%s | %s] %s", stamp, code, msg);
                    FTI_Print(str, FTI_WARN);
                } else {
                    FTI_Print("Event message with bad formatting.", FTI_WARN);
                }
                free(tofree);
            }
        }
        FTI_Noti.position = ftell(fh);
        FTI_Noti.size = size;
    }
    return FTI_SCES;
}


