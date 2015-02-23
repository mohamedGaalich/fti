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
#define     FTI_MXRL    10


int FTI_RULE[FTI_MXRL][6] =
{
  //cmp err cnt lvl fqn intv
    {01, 54, 00, 04, 02, 001},
    {-1, -1, -1, -1, -1, -01},
    {-1, -1, -1, -1, -1, -01},
    {-1, -1, -1, -1, -1, -01},
    {-1, -1, -1, -1, -1, -01},
    {-1, -1, -1, -1, -1, -01},
    {-1, -1, -1, -1, -1, -01},
    {-1, -1, -1, -1, -1, -01},
    {-1, -1, -1, -1, -1, -01},
    {-1, -1, -1, -1, -1, -01},
};

int FTI_DecodeNoti(int code, int rl[6])
{
    int i, found = 0;
    if ((code >= 0) && (code < 1000000))
    {
        rl[0] = code / 100000;
        rl[1] = (code-(rl[0]*100000))/1000;
        rl[2] = (code-(rl[0]*100000))-(rl[1]*1000);
        for (i = 0; i < FTI_MXRL; i++)
        {
            if ((rl[0] == FTI_RULE[i][0]) && (rl[1] == FTI_RULE[i][1]) && (rl[2] >= FTI_RULE[i][2]))
            {
                rl[3] = FTI_RULE[i][3];
                rl[4] = FTI_RULE[i][4];
                rl[5] = FTI_RULE[i][5];
                found = 1;
            }
        }
        if (found)
        {
            return FTI_SCES;
        } else {
            FTI_Print("No action has been set for this kind of event.", FTI_WARN);
            return FTI_NSCS;
        }
    } else {
        FTI_Print("Notification numeric code out of bounds.", FTI_WARN);
        return FTI_NSCS;
    }
}

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
    char noti[FTI_MXNT][FTI_BUFS], str[FTI_BUFS];
    int i, cnt, flag, code, rule[6] = {-1, -1, -1, -1, -1, -1};
    MPI_Status status;
    cnt = FTI_CheckNoti(noti);
    if (cnt > 0)
    {
        for (i = 0; i < cnt; i++)
        {
            code = FTI_AnalyzeNoti(noti[i]);
            if (code >= 0)
            {
                if (FTI_DecodeNoti(code, rule) == FTI_SCES)
                {
                    int lv = rule[3];
                    FTI_Ckpt[lv].ckptIntv = (int)(FTI_Ckpt[lv].baseIntv / rule[4]);
                    FTI_Ckpt[lv].regStart = FTI_Exec.ckptIcnt;
                    FTI_Ckpt[lv].regStopt = FTI_Ckpt[lv].regStart + (rule[5] * FTI_Exec.ckptIntv);
                    sprintf(str, "Event #%d in component #%d with %d ocurrences.", rule[1], rule[0], rule[2]);
                    FTI_Print(str, FTI_WARN);
                    sprintf(str, "%dX increment in L%d ckpt. frequency during %d min.", rule[4], rule[3], rule[5]);
                    FTI_Print(str, FTI_WARN);
                    sprintf(str, "Base ckpt. interval for L%d is %d, current one is %d", lv, FTI_Ckpt[lv].baseIntv, FTI_Ckpt[lv].ckptIntv);
                    FTI_Print(str, FTI_WARN);
                    sprintf(str, "Changed made at iteration %d to be reverted in iteration %d", FTI_Ckpt[lv].regStart, FTI_Ckpt[lv].regStopt);
                    FTI_Print(str, FTI_WARN);
                }
            }
        }
    }
    for(i = 1; i < 5; i++)
    {
        if (FTI_Ckpt[i].baseIntv != FTI_Ckpt[i].ckptIntv)
        {
            if (FTI_Ckpt[i].regStopt <= FTI_Exec.ckptIcnt)
            {
                FTI_Ckpt[i].ckptIntv = FTI_Ckpt[i].baseIntv;
                FTI_Ckpt[i].regStart = 0;
                FTI_Ckpt[i].regStopt = 0;
                sprintf(str, "Ckpt. interval L%d reverted at iteration %d", i, FTI_Exec.ckptIcnt);
                FTI_Print(str, FTI_WARN);
            }
        }
    }
    return FTI_SCES;
}


