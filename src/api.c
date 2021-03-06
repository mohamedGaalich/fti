/**
 *  @file   api.c
 *  @author Leonardo A. Bautista Gomez (leobago@gmail.com)
 *  @date   December, 2013
 *  @brief  API functions for the FTI library.
 */


#include "fti.h"


/** Array of datasets and all their internal information.                  */
static FTIT_dataset        FTI_Data[FTI_BUFS];

/** SDC injection model and all the required information.                  */
static FTIT_injection      FTI_Inje;


/*-------------------------------------------------------------------------*/
/**
    @brief      It aborts the application.

    This function aborts the application after cleaning the file system.

 **/
/*-------------------------------------------------------------------------*/
void FTI_Abort() {
        FTI_Clean(5, 0, FTI_Topo.myRank);
        MPI_Abort(MPI_COMM_WORLD, -1);
        MPI_Finalize();
        exit(1);
}


/*-------------------------------------------------------------------------*/
/**
    @brief      Initializes FTI.
    @param      configFile      FTI configuration file.
    @param      globalComm      Main MPI communicator of the application.
    @return     integer         FTI_SCES if successful.

    This function initialize the FTI context and prepare the heads to wait
    for checkpoints. FTI processes should never get out of this function. In
    case of restart, checkpoint files should be recovered and in place at the
    end of this function.

 **/
/*-------------------------------------------------------------------------*/
int FTI_Init(char *configFile, MPI_Comm globalComm) {
    FTI_Exec.globalComm = globalComm;
    MPI_Comm_rank(FTI_Exec.globalComm, &FTI_Topo.myRank);
    MPI_Comm_size(FTI_Exec.globalComm, &FTI_Topo.nbProc);
    snprintf(FTI_Conf.cfgFile, FTI_BUFS, "%s", configFile);
    FTI_Conf.verbosity = 1;
    FTI_Inje.timer = MPI_Wtime();
    FTI_COMM_WORLD = globalComm;            // Temporary before building topology
    FTI_Topo.splitRank = FTI_Topo.myRank;   // Temporary before building topology
    int res = FTI_Try(FTI_LoadConf(&FTI_Inje), "load configuration.");
    if (res == FTI_NSCS) FTI_Abort();
    res = FTI_Try(FTI_Topology(), "build topology.");
    if (res == FTI_NSCS) FTI_Abort();
    FTI_Try(FTI_InitBasicTypes(FTI_Data), "create the basic data types.");
    if (FTI_Topo.myRank == 0) FTI_Try(FTI_UpdateConf(1), "update configuration file.");
    if (FTI_Topo.amIaHead)
    { // If I am a FTI dedicated process
        if (FTI_Exec.reco)
        {
            res = FTI_Try(FTI_RecoverFiles(), "recover the checkpoint files.");
            if (res == FTI_NSCS) FTI_Abort();
        }
        res = 0;
        while (res != FTI_ENDW) {
            res = FTI_Listen();
        }
        FTI_Print("Head stopped listening.", FTI_DBUG);
        FTI_Finalize();
    } else { // If I am an application process
        if (FTI_Exec.reco)
        {
            res = FTI_Try(FTI_RecoverFiles(), "recover the checkpoint files.");
            if (res == FTI_NSCS) FTI_Abort();
            FTI_Exec.ckptCnt = FTI_Exec.ckptID;
        }
    }
    FTI_Print("FTI has been initialized.", FTI_INFO);
    return FTI_SCES;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It returns the current status of the recovery flag.
    @return     integer         FTI_Exec.reco

    This function returns the current status of the recovery flag.

 **/
/*-------------------------------------------------------------------------*/
int FTI_Status() {
    return FTI_Exec.reco;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It initializes a data type.
    @param      type            The data type to be intialized.
    @param      size            The size of the data type to be intialized.
    @return     integer         FTI_SCES if successful.

    This function initalizes a data type. the only information needed is the
    size of the data type, the rest is black box for FTI.

 **/
/*-------------------------------------------------------------------------*/
int FTI_InitType(FTIT_type *type, int size) {
    type->id = FTI_Exec.nbType;
    type->size = size;
    FTI_Exec.nbType = FTI_Exec.nbType + 1;
    return FTI_SCES;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It sets/resets the pointer and type to a protected variable.
    @param      id              ID for searches and update.
    @param      ptr             Pointer to the data structure.
    @param      count           Number of elements in the data structure.
    @param      type            Type of elements in the data structure.
    @return     integer         FTI_SCES if successful.

    This function stores a pointer to a data structure, its size, its ID,
    its number of elements and the type of the elements. This list of
    structures is the data that will be stored during a checkpoint and
    loaded during a recovery. It resets the pointer to a data structure,
    its size, its number of elements and the type of the elements if the
    dataset was already previously registered.

 **/
/*-------------------------------------------------------------------------*/
int FTI_Protect(int id, void *ptr, long count, FTIT_type type) {
    int i, prevSize, updated = 0;
    char str[FTI_BUFS];
    float ckptSize;
    for (i = 0; i < FTI_BUFS; i++)
    {
        if (id == FTI_Data[i].id)
        {
            prevSize = FTI_Data[i].size;
            FTI_Data[i].ptr = ptr;
            FTI_Data[i].count = count;
            FTI_Data[i].type = type;
            FTI_Data[i].eleSize = type.size;
            FTI_Data[i].size = type.size*count;
            FTI_Exec.ckptSize = FTI_Exec.ckptSize + (type.size*count) - prevSize;
            updated = 1;
        }
    }
    if (updated)
    {
        ckptSize = FTI_Exec.ckptSize/(1024.0*1024.0);
        sprintf(str, "Variable ID %d reseted. Current ckpt. size per rank is %.2fMB.", id, ckptSize);
        FTI_Print(str, FTI_DBUG);
    } else {
        if (FTI_Exec.nbVar >= FTI_BUFS)
        {
            FTI_Print("Too many variables registered.", FTI_EROR);
            FTI_Clean(5, FTI_Topo.groupID, FTI_Topo.myRank);
            MPI_Abort(MPI_COMM_WORLD, -1);
            MPI_Finalize();
            exit(1);
        }
        FTI_Data[FTI_Exec.nbVar].id = id;
        FTI_Data[FTI_Exec.nbVar].ptr = ptr;
        FTI_Data[FTI_Exec.nbVar].count = count;
        FTI_Data[FTI_Exec.nbVar].type = type;
        FTI_Data[FTI_Exec.nbVar].eleSize = type.size;
        FTI_Data[FTI_Exec.nbVar].size = type.size*count;
        FTI_Exec.nbVar = FTI_Exec.nbVar + 1;
        FTI_Exec.ckptSize = FTI_Exec.ckptSize + (type.size*count);
        ckptSize = FTI_Exec.ckptSize/(1024.0*1024.0);
        sprintf(str, "Variable ID %d to protect. Current ckpt. size per rank is %.2fMB.", id, ckptSize);
        FTI_Print(str, FTI_INFO);
    }
    return FTI_SCES;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It corrupts a bit of the given float.
    @param      target          Pointer to the float to corrupt.
    @param      bit             Position of the bit to corrupt.
    @return     integer         FTI_SCES if successfull.

    This function filps the bit of the target float.

 **/
/*-------------------------------------------------------------------------*/
int FTI_FloatBitFlip(float *target, int bit)
{
    if (bit >= 32 || bit < 0)
    {
        return FTI_NSCS;
    }
    int *corIntPtr = (int*)target;
    int corInt = *corIntPtr;
    corInt = corInt ^ (1 << bit);
    corIntPtr = &corInt;
    float *fp = (float*)corIntPtr;
    *target = *fp;
    return FTI_SCES;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It corrupts a bit of the given float.
    @param      target          Pointer to the float to corrupt.
    @param      bit             Position of the bit to corrupt.
    @return     integer         FTI_SCES if successfull.

    This function filps the bit of the target float.

 **/
/*-------------------------------------------------------------------------*/
int FTI_DoubleBitFlip(double *target, int bit)
{
    if (bit >= 64 || bit < 0)
    {
        return FTI_NSCS;
    }
    FTIT_double myDouble;
    myDouble.value = *target;
    int bitf = (bit >= 32) ? bit-32 : bit;
    int half = (bit >= 32) ? 1 : 0;
    FTI_FloatBitFlip(&(myDouble.floatval[half]), bitf);
    *target = myDouble.value;
    return FTI_SCES;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      Bit-flip injection following the injection instructions.
    @param      datasetID       ID of the dataset where to inject.
    @return     integer         FTI_SCES if successfull.

    This function injects the given number of bit-flips, at the given
    frequency and in the given location (rank, dataset, bit position).

 **/
/*-------------------------------------------------------------------------*/
int FTI_BitFlip(int datasetID)
{
    if (FTI_Inje.rank == FTI_Topo.splitRank)
    {
        if (datasetID >= FTI_Exec.nbVar)
        {
            return FTI_NSCS;
        }
        if (FTI_Inje.counter < FTI_Inje.number)
        {
            if ((MPI_Wtime()-FTI_Inje.timer) > FTI_Inje.frequency)
            {
                if (FTI_Inje.index < FTI_Data[datasetID].count)
                {
                    char str[FTI_BUFS];
                    if (FTI_Data[datasetID].type.id == 9)
                    { // If it is a double
                        double *target = FTI_Data[datasetID].ptr+FTI_Inje.index;
                        double ori = *target;
                        int res = FTI_DoubleBitFlip(target, FTI_Inje.position);
                        FTI_Inje.counter = (res == FTI_SCES) ? FTI_Inje.counter + 1 : FTI_Inje.counter;
                        FTI_Inje.timer = (res == FTI_SCES) ? MPI_Wtime() : FTI_Inje.timer;
                        sprintf(str, "Injecting bit-flip in dataset %d, index %d, bit %d : %f => %f",
                                datasetID, FTI_Inje.index, FTI_Inje.position, ori, *target);
                        FTI_Print(str, FTI_WARN);
                        return res;
                    }
                    if (FTI_Data[datasetID].type.id == 8)
                    { // If it is a float
                        float *target = FTI_Data[datasetID].ptr+FTI_Inje.index;
                        float ori = *target;
                        int res = FTI_FloatBitFlip(target, FTI_Inje.position);
                        FTI_Inje.counter = (res == FTI_SCES) ? FTI_Inje.counter + 1 : FTI_Inje.counter;
                        FTI_Inje.timer = (res == FTI_SCES) ? MPI_Wtime() : FTI_Inje.timer;
                        sprintf(str, "Injecting bit-flip in dataset %d, index %d, bit %d : %f => %f",
                                datasetID, FTI_Inje.index, FTI_Inje.position, ori, *target);
                        FTI_Print(str, FTI_WARN);
                        return res;
                    }
                }
            }
        }
    }
    return FTI_NSCS;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It takes the checkpoint and triggers the post-ckpt. work.
    @param      id              Checkpoint ID.
    @param      level           Checkpoint level.
    @return     integer         FTI_SCES if successfull.

    This function starts by blocking on a receive if the previous ckpt. was
    offline. Then, it updates the ckpt. information. It writes down the ckpt.
    data, creates the metadata and the post-processing work. This function
    is complementary with the FTI_Listen function in terms of communications.

 **/
/*-------------------------------------------------------------------------*/
int FTI_Checkpoint(int id, int level) {
    int i, res = FTI_NSCS;
    double t0, t1, t2, t3, t4;
    char str[FTI_BUFS];
    MPI_Status status;
    if ((level > 0) && (level < 5))
    {
        t0 = MPI_Wtime();
        FTI_Exec.ckptID = id;
        FTI_Exec.ckptLvel = level;
        sprintf(str, "Ckpt. ID %d", FTI_Exec.ckptID);
        sprintf(str, "%s (L%d) (%.2f MB/proc)", str, FTI_Exec.ckptLvel, FTI_Exec.ckptSize/(1024.0*1024.0));
        if (FTI_Exec.wasLastOffline == 1)
        { // Block until previous checkpoint is done (Async. work)
            MPI_Recv(&res, 1, MPI_INT, FTI_Topo.headRank, FTI_Conf.tag, FTI_Exec.globalComm, &status);
            if (res == FTI_SCES)
            {
                FTI_Exec.lastCkptLvel = res;
                FTI_Exec.wasLastOffline = 1;
                FTI_Exec.lastCkptLvel = FTI_Exec.ckptLvel;
            }
        }
        t1 = MPI_Wtime();
        res = FTI_Try(FTI_WriteCkpt(FTI_Data), "write the checkpoint.");
        //MPI_Allreduce(&res, &tres, 1, MPI_INT, MPI_SUM, FTI_COMM_WORLD);
        t2 = MPI_Wtime();
        if (!FTI_Ckpt[FTI_Exec.ckptLvel].isInline)
        { // If postCkpt. work is Async. then send message..
            FTI_Exec.wasLastOffline = 1;
            if (res != FTI_SCES)
            {
                res = FTI_REJW;
            } else {
                res = FTI_BASE + FTI_Exec.ckptLvel;
            }
            MPI_Send(&res, 1, MPI_INT, FTI_Topo.headRank, FTI_Conf.tag, FTI_Exec.globalComm);
        } else {
            FTI_Exec.wasLastOffline = 0;
            if (res != FTI_SCES) FTI_Exec.ckptLvel = FTI_REJW-FTI_BASE;
            res = FTI_Try(FTI_PostCkpt(FTI_Topo.groupID, -1, 1), "postprocess the checkpoint.");
            if (res == FTI_SCES)
            {
                FTI_Exec.wasLastOffline = 0;
                FTI_Exec.lastCkptLvel = FTI_Exec.ckptLvel;
            }
        }
        t3 = MPI_Wtime();
        sprintf(str, "%s taken in %.2f sec.", str, t3-t0);
        sprintf(str, "%s (Wt:%.2fs, Wr:%.2fs, Ps:%.2fs)", str, t1-t0, t2-t1, t3-t2);
        FTI_Print(str, FTI_INFO);
        if (res == FTI_SCES) res = FTI_DONE;
        else res = FTI_NSCS;
    }
    return res;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It loads the checkpoint data.
    @return     integer         FTI_SCES if successful.

    This function loads the checkpoint data from the checkpoint file and
    it updates some basic checkpoint information.

 **/
/*-------------------------------------------------------------------------*/
int FTI_Recover() {
    char fn[FTI_BUFS], str[FTI_BUFS];
    FILE *fd;
    int i;
    sprintf(fn,"%s/%s" ,FTI_Ckpt[FTI_Exec.ckptLvel].dir, FTI_Exec.ckptFile);
    sprintf(str, "Trying to load FTI checkpoint file (%s)...", fn);
    FTI_Print(str, FTI_DBUG);
    if (access(fn, F_OK) != 0)
    {
        FTI_Print("FTI checkpoint file is NOT accesible.", FTI_EROR);
        return FTI_NSCS;
    }
    fd = fopen(fn, "rb");
    if (fd == NULL)
    {
        FTI_Print("Could not open FTI checkpoint file.", FTI_EROR);
        return FTI_NSCS;
    }
    for(i = 0; i < FTI_Exec.nbVar; i++)
    {
        fread(FTI_Data[i].ptr, 1, FTI_Data[i].size, fd);
    }
    if (fclose(fd) != 0)
    {
        FTI_Print("Could not close FTI checkpoint file.", FTI_EROR);
        return FTI_NSCS;
    }
    FTI_Exec.reco = 0;
    return FTI_SCES;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      Takes an FTI snapshot or recover the data if it is a restart.
    @return     integer         FTI_SCES if successful.

    This function loads the checkpoint data from the checkpoint file in case
    of restart. Otherwise, it checks if the current iteration requires
    checkpointing, if it does it checks which checkpoint level, write the
    data in the files and it communicates with the head of the node to inform
    that a checkpoint has been taken. Checkpoint ID and counters are updated.

 **/
/*-------------------------------------------------------------------------*/
int FTI_Snapshot() {
    int i, res, level = -1;
    if (FTI_Exec.reco)
    { // If this is a recovery load icheckpoint data
        res = FTI_Try(FTI_Recover(), "recover the checkpointed data.");
        if (res == FTI_NSCS)
        {
            FTI_Print("Impossible to load the checkpoint data.", FTI_EROR);
            FTI_Clean(5, FTI_Topo.groupID, FTI_Topo.myRank);
            MPI_Abort(MPI_COMM_WORLD, -1);
            MPI_Finalize();
            exit(1);
        }
    } else { // If it is a checkpoint test
        res = FTI_SCES;
        FTI_UpdateIterTime();
        if (FTI_Exec.ckptNext == FTI_Exec.ckptIcnt)
        { // If it is time to check for possible ckpt. (every minute)
            FTI_Print("Checking if it is time to checkpoint.", FTI_DBUG);
            FTI_Exec.ckptCnt++; // Increment minute counter
            for (i = 1; i < 5; i++)
            { // Check ckpt. level
                if (FTI_Exec.ckptCnt % FTI_Ckpt[i].ckptIntv == 0)
                {
                    level = i;
                }
            }
            if (level != -1)
            {
                    res = FTI_Try(FTI_Checkpoint(FTI_Exec.ckptCnt, level), "take checkpoint.");
            }
            FTI_Exec.ckptLast = FTI_Exec.ckptNext;
            FTI_Exec.ckptNext = FTI_Exec.ckptNext + FTI_Exec.ckptIntv;
            FTI_Exec.iterTime = MPI_Wtime(); // Reset iteration duration timer
        }
    }
    return res;
}


/*-------------------------------------------------------------------------*/
/**
    @brief      It closes FTI properly on the application processes.
    @return     integer         FTI_SCES if successful.

    This function notify the FTI processes that the execution is over, frees
    some data structures and it closes. If this function is not called on the
    application processes the FTI processes will never finish (deadlock).

 **/
/*-------------------------------------------------------------------------*/
int FTI_Finalize() {
    if (!FTI_Topo.amIaHead)
    {
        int buff = FTI_ENDW;
        MPI_Status status;
        if (FTI_Exec.wasLastOffline == 1)
        { // If there is remaining work to do for last checkpoint
            MPI_Recv(&buff, 1, MPI_INT, FTI_Topo.headRank, FTI_Conf.tag, FTI_Exec.globalComm, &status);
            if (buff != FTI_NSCS)
            {
                FTI_Exec.ckptLvel = buff;
                FTI_Exec.wasLastOffline = 1;
                FTI_Exec.lastCkptLvel = FTI_Exec.ckptLvel;
            }
        }
        buff = FTI_ENDW;
        if (FTI_Topo.nbHeads == 1)
        { // Send notice to the head to stop listening
            MPI_Send(&buff, 1, MPI_INT, FTI_Topo.headRank, FTI_Conf.tag, FTI_Exec.globalComm);
        }
        if (FTI_Conf.saveLastCkpt)
        { // If we need to keep the last checkpoint
            if (FTI_Exec.lastCkptLvel != 4)
            {
                FTI_Try(FTI_Flush(FTI_Topo.groupID, FTI_Exec.lastCkptLvel), "save the last ckpt. in the PFS.");
                MPI_Barrier(FTI_COMM_WORLD);
                if (FTI_Topo.splitRank == 0)
		{
                    if (access(FTI_Ckpt[4].dir,0)==0)
                        FTI_RmDir(FTI_Ckpt[4].dir, 1);
                    if (access(FTI_Ckpt[4].metaDir,0)==0)
                        FTI_RmDir(FTI_Ckpt[4].metaDir, 1);
                    rename(FTI_Ckpt[FTI_Exec.lastCkptLvel].metaDir, FTI_Ckpt[4].metaDir);
                    rename(FTI_Conf.gTmpDir, FTI_Ckpt[4].dir);
                }
            }
            if (FTI_Topo.splitRank == 0)
            {
                FTI_Try(FTI_UpdateConf(2), "update configuration file to 2.");
            }
            buff = 6; // For cleaning only local storage
        } else {
            if (FTI_Topo.splitRank == 0)
            {
                FTI_Try(FTI_UpdateConf(0), "update configuration file to 0.");
            }
            buff = 5; // For cleaning everything
        }
        MPI_Barrier(FTI_Exec.globalComm);
        FTI_Try(FTI_Clean(buff, FTI_Topo.groupID, FTI_Topo.myRank), "do final clean.");
        FTI_Print("FTI has been finalized.", FTI_INFO);
    } else {
        MPI_Barrier(FTI_Exec.globalComm);
        MPI_Finalize();
        exit(0);
    }
    return FTI_SCES;
}


