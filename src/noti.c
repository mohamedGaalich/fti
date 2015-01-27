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

#define     FTI_MXNT    3


/*-------------------------------------------------------------------------*/
/**
    @brief      It extracts the code of the notification.
    @param      noti            Notifications to analyze.
    @return     integer         The code of the notification or -1 if error.

    This function checks the notification and extracts its code and returns
    it as an integer.

 **/
/*-------------------------------------------------------------------------*/
int FTI_AnalyzeNoti(char noti[FTI_BUFS])
{
    int coded;
    if (noti != NULL)
    {
        char *buf, *tofree, *stamp, *code, *msg, str[FTI_BUFS];
        buf = strdup(noti);
        tofree = buf;
        stamp = strsep(&buf, "|");
        code = strsep(&buf, "|");
        msg = strsep(&buf, "|");
        if ((stamp != NULL) && (code != NULL) && (msg != NULL))
        {
            if (msg[strlen(msg)-1] == '\n')
            {
                msg[strlen(msg)-1] = ' ';
            }
            sprintf(str, "[%s | %s] %s", stamp, code, msg);
            FTI_Print(str, FTI_WARN);
            if(strlen(code) == 6)
            {
                FTI_Print("Analyzing notification code.", FTI_WARN);
                coded = atoi(code);
            } else {
                FTI_Print("Wrong notification code.", FTI_WARN);
                coded = -1;
            }
        } else {
            FTI_Print("Notification message with bad formatting.", FTI_WARN);
            coded = -1;
        }
        free(tofree);
    }
    return coded;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It gets the metadata to recover the data after a failure.
    @param      noti            Buffer where notifications are returned.
    @return     integer         The number of notifications returned.

    This function checks the notification file for new notifications. It
    reads up all the notifications present in the file since the last check,
    but it only return the last FTI_MXNT notifications (not necessarily in
    the correct order of arrival).

 **/
/*-------------------------------------------------------------------------*/
int FTI_CheckNoti(char noti[FTI_MXNT][FTI_BUFS])
{
    struct stat st;
    int ind = 0, cnt = 0, size = 0;
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
            if (fgets(noti[ind], FTI_BUFS, fh) != NULL)
            {
                ind = ind + 1;
                cnt = cnt + 1;
                if (ind >= FTI_MXNT)
                {
                    ind = 0;
                    FTI_Print("Too many notifications received, overwriting previous ones", FTI_WARN);
                }
            }
        }
        if (cnt > FTI_MXNT) cnt = FTI_MXNT;
        FTI_Noti.position = ftell(fh);
        FTI_Noti.size = size;
    }
    return cnt;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It manages systems notifications.
    @return     integer         FTI_SCES if successful.

    This function draws the logic to follow to check, analyze and react to
    system notifications.

 **/
/*-------------------------------------------------------------------------*/
int FTI_GetNoti()
{
    char noti[FTI_MXNT][FTI_BUFS];
    int i, code, cnt = FTI_CheckNoti(noti);
    if (cnt > 0)
    {
        for (i = 0; i < cnt; i++)
        {
            code = FTI_AnalyzeNoti(noti[i]);
            if ((code > 0) && (code < 1000000))
            {
                int cmp = code / 100000;
                int err = (code-(cmp*100000))/1000;
                int nbr = (code-(cmp*100000))-(err*1000);
                printf("code %d cmp %d err %d nbr %d \n", code, cmp, err, nbr);
            }
        }
    }
    return FTI_SCES;
}


