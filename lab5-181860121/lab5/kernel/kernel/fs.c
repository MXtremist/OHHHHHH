#include "x86.h"
#include "device.h"
#include "fs.h"

int readSuperBlock(SuperBlock *superBlock)
{
    diskRead((void *)superBlock, sizeof(SuperBlock), 1, 0);
    return 0;
}

int readBlock(SuperBlock *superBlock, Inode *inode, int blockIndex, uint8_t *buffer)
{
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];

    if (blockIndex < bound0)
    {
        diskRead((void *)buffer, sizeof(uint8_t), superBlock->blockSize, inode->pointer[blockIndex] * SECTOR_SIZE);
        return 0;
    }
    else if (blockIndex < bound1)
    {
        diskRead((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
        diskRead((void *)buffer, sizeof(uint8_t), superBlock->blockSize, singlyPointerBuffer[blockIndex - bound0] * SECTOR_SIZE);
        return 0;
    }
    else
    {
        return -1;
    }
}

int writeBlock(SuperBlock *superBlock, Inode *inode, int blockIndex, uint8_t *buffer)
{
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];

    if (blockIndex < bound0)
    {
        diskWrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, inode->pointer[blockIndex] * SECTOR_SIZE);
        return 0;
    }
    else if (blockIndex < bound1)
    {
        diskRead((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
        diskWrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, singlyPointerBuffer[blockIndex - bound0] * SECTOR_SIZE);
        return 0;
    }
    else
        return -1;
}

int readInode(SuperBlock *superBlock, Inode *destInode, int *inodeOffset, const char *destFilePath)
{
    int i = 0;
    int j = 0;
    int ret = 0;
    int cond = 0;
    *inodeOffset = 0;
    uint8_t buffer[superBlock->blockSize];
    DirEntry *dirEntry = NULL;
    int count = 0;
    int size = 0;
    int blockCount = 0;

    if (destFilePath == NULL || destFilePath[count] == 0)
    {
        return -1;
    }
    ret = stringChr(destFilePath, '/', &size);
    if (ret == -1 || size != 0)
    {
        return -1;
    }
    count += 1;
    *inodeOffset = superBlock->inodeTable * SECTOR_SIZE;
    diskRead((void *)destInode, sizeof(Inode), 1, *inodeOffset);

    while (destFilePath[count] != 0)
    {
        ret = stringChr(destFilePath + count, '/', &size);
        if (ret == 0 && size == 0)
        {
            return -1;
        }
        if (ret == -1)
        {
            cond = 1;
        }
        else if (destInode->type == REGULAR_TYPE)
        {
            return -1;
        }
        blockCount = destInode->blockCount;
        for (i = 0; i < blockCount; i++)
        {
            ret = readBlock(superBlock, destInode, i, buffer);
            if (ret == -1)
            {
                return -1;
            }
            dirEntry = (DirEntry *)buffer;
            for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j++)
            {
                if (dirEntry[j].inode == 0)
                {
                    continue;
                }
                else if (stringCmp(dirEntry[j].name, destFilePath + count, size) == 0)
                {
                    *inodeOffset = superBlock->inodeTable * SECTOR_SIZE + (dirEntry[j].inode - 1) * sizeof(Inode);
                    diskRead((void *)destInode, sizeof(Inode), 1, *inodeOffset);
                    break;
                }
            }
            if (j < superBlock->blockSize / sizeof(DirEntry))
            {
                break;
            }
        }
        if (i < blockCount)
        {
            if (cond == 0)
            {
                count += (size + 1);
            }
            else
            {
                return 0;
            }
        }
        else
        {
            return -1;
        }
    }
    return 0;
}

int allocBlock(SuperBlock *superBlock, Inode *inode, int inodeOffset)
{
    int ret = 0;
    int blockOffset = 0;

    ret = calNeededPointerBlocks(superBlock, inode->blockCount);
    if (ret == -1)
        return -1;
    if (superBlock->availBlockNum < ret + 1)
        return -1;

    getAvailBlock(superBlock, &blockOffset);
    allocLastBlock(superBlock, inode, inodeOffset, blockOffset);
    return 0;
}

int allocInode(SuperBlock *superBlock, Inode *fatherInode, int fatherInodeOffset, Inode *destInode, int *destInodeOffset, const char *destFilename, int destFiletype)
{
    int i = 0;
    int j = 0;
    int ret = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];
    int length = stringLen(destFilename);

    if (destFilename == NULL || destFilename[0] == 0)
        return -1;

    if (superBlock->availInodeNum == 0)
        return -1;

    for (i = 0; i < fatherInode->blockCount; i++)
    {
        ret = readBlock(superBlock, fatherInode, i, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j++)
        {
            if (dirEntry[j].inode == 0) // a valid empty dirEntry
                break;
            else if (stringCmp(dirEntry[j].name, destFilename, length) == 0)
                return -1; // file with filename = destFilename exist
        }
        if (j < superBlock->blockSize / sizeof(DirEntry))
            break;
    }
    if (i == fatherInode->blockCount)
    {
        ret = allocBlock(superBlock, fatherInode, fatherInodeOffset);
        if (ret == -1)
            return -1;
        fatherInode->size = fatherInode->blockCount * superBlock->blockSize;
        setBuffer(buffer, superBlock->blockSize, 0);
        dirEntry = (DirEntry *)buffer;
        j = 0;
    }
    // dirEntry[j] is the valid empty dirEntry, it is in the i-th block of fatherInode.
    ret = getAvailInode(superBlock, destInodeOffset);
    if (ret == -1)
        return -1;

    stringCpy(destFilename, dirEntry[j].name, NAME_LENGTH);
    dirEntry[j].inode = (*destInodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) + 1;
    ret = writeBlock(superBlock, fatherInode, i, buffer);
    if (ret == -1)
        return -1;
    diskWrite((void *)fatherInode, sizeof(Inode), 1, fatherInodeOffset);

    destInode->type = destFiletype;
    destInode->linkCount = 1;
    destInode->blockCount = 0;
    destInode->size = 0;

    diskWrite((void *)destInode, sizeof(Inode), 1, *destInodeOffset);

    return 0;
}

int getDirEntry(SuperBlock *superBlock, Inode *inode, int dirIndex, DirEntry *destDirEntry)
{
    int i = 0;
    int j = 0;
    int ret = 0;
    int dirCount = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];

    for (i = 0; i < inode->blockCount; i++)
    {
        ret = readBlock(superBlock, inode, i, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j++)
        {
            if (dirEntry[j].inode != 0)
            {
                if (dirCount == dirIndex)
                    break;
                else
                    dirCount++;
            }
        }
        if (j < superBlock->blockSize / sizeof(DirEntry))
            break;
    }
    if (i == inode->blockCount)
        return -1;
    else
    {
        destDirEntry->inode = dirEntry[j].inode;
        stringCpy(dirEntry[j].name, destDirEntry->name, NAME_LENGTH);
        return 0;
    }
}

int initDir(SuperBlock *superBlock, Inode *fatherInode, int fatherInodeOffset,
            Inode *destInode, int destInodeOffset)
{
    int ret = 0;
    int blockOffset = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];

    ret = getAvailBlock(superBlock, &blockOffset);
    if (ret == -1)
        return -1;
    destInode->pointer[0] = blockOffset;
    destInode->blockCount = 1;
    destInode->size = superBlock->blockSize;
    setBuffer(buffer, superBlock->blockSize, 0);
    dirEntry = (DirEntry *)buffer;
    dirEntry[0].inode = (destInodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) + 1;
    destInode->linkCount++;
    dirEntry[0].name[0] = '.';
    dirEntry[0].name[1] = '\0';
    dirEntry[1].inode = (fatherInodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) + 1;
    fatherInode->linkCount++;
    dirEntry[1].name[0] = '.';
    dirEntry[1].name[1] = '.';
    dirEntry[1].name[2] = '\0';

    diskWrite((void *)buffer, sizeof(uint8_t), superBlock->blockSize, blockOffset * SECTOR_SIZE);
    diskWrite((void *)fatherInode, sizeof(Inode), 1, fatherInodeOffset);
    diskWrite((void *)destInode, sizeof(Inode), 1, destInodeOffset);
    return 0;
}

int mkdir(const char *destDirPath)
{
    char tmp = 0;
    int length = 0;
    int cond = 0;
    int ret = 0;
    int size = 0;
    SuperBlock superBlock;
    int fatherInodeOffset = 0;
    int destInodeOffset = 0;
    Inode fatherInode;
    Inode destInode;
    ret = readSuperBlock(&superBlock);
    if (ret == -1)
    {
        putString("Failed to load SuperBlock.\n");
        return -1;
    }
    if (destDirPath == NULL)
    {
        putString("destDirPath == NULL");
        return -1;
    }
    length = stringLen(destDirPath);
    if (destDirPath[length - 1] == '/')
    {
        cond = 1;
        *((char *)destDirPath + length - 1) = 0;
    }
    ret = stringChrR(destDirPath, '/', &size);
    if (ret == -1)
    {
        putString("Incorrect destination file path.\n");
        return -1;
    }
    tmp = *((char *)destDirPath + size + 1);
    *((char *)destDirPath + size + 1) = 0;
    ret = readInode(&superBlock, &fatherInode, &fatherInodeOffset, destDirPath);
    *((char *)destDirPath + size + 1) = tmp;
    if (ret == -1)
    {
        putString("Failed to read father inode.\n");
        if (cond == 1)
            *((char *)destDirPath + length - 1) = '/';
        return -1;
    }
    ret = allocInode(&superBlock, &fatherInode, fatherInodeOffset,
                     &destInode, &destInodeOffset, destDirPath + size + 1, DIRECTORY_TYPE);
    if (ret == -1)
    {
        putString("Failed to allocate inode.\n");
        if (cond == 1)
            *((char *)destDirPath + length - 1) = '/';
        return -1;
    }
    ret = initDir(&superBlock, &fatherInode, fatherInodeOffset,
                  &destInode, destInodeOffset);
    if (ret == -1)
    {
        putString("Failed to initialize dir.\n");
        if (cond == 1)
            *((char *)destDirPath + length - 1) = '/';
        return -1;
    }
    if (cond == 1)
        *((char *)destDirPath + length - 1) = '/';
    return 0;
}

int myremove(Inode *inode, Inode *fatherinode, SuperBlock *superBlock, int fatherInodeOffset, int inodeOffset)
{
    //free inode
    int inodeTableOffset = superBlock->inodeTable;
    int inodeBitmapOffset = superBlock->inodeBitmap;
    InodeBitmap inodeBitmap;
    diskRead((void *)&inodeBitmap, sizeof(InodeBitmap), 1, inodeBitmapOffset * SECTOR_SIZE);
    int i = (inodeOffset - inodeTableOffset * SECTOR_SIZE) / sizeof(Inode);
    int j = i / 8;
    int k = i % 8;
    inodeBitmap.byte[j] ^= (1 << (7 - k));
    superBlock->availInodeNum++;
    diskWrite((void *)&inodeBitmap, sizeof(InodeBitmap), 1, inodeBitmapOffset * SECTOR_SIZE);

    //free blocks
    int blockBitmapOffset = superBlock->blockBitmap;
    BlockBitmap blockBitmap;
    diskRead((void *)&blockBitmap, sizeof(BlockBitmap), 1, blockBitmapOffset * SECTOR_SIZE);
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;
    uint32_t singlyPointerBuffer[divider0];
    for (int index = 0; index < inode->blockCount; index++)
    {
        if (index < bound0)
        {
            int insideblockOffset = (inode->pointer[index] - superBlock->blocks) * SECTOR_SIZE;
            int i = (insideblockOffset) / superBlock->blockSize;
            int j = i / 8;
            int k = i % 8;
            blockBitmap.byte[j] ^= (1 << (7 - k));
        }
        else if (index < bound1)
        {
            diskRead((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
            int insideblockOffset = (singlyPointerBuffer[index - bound0] - superBlock->blocks) * SECTOR_SIZE;
            int i = (insideblockOffset) / superBlock->blockSize;
            int j = i / 8;
            int k = i % 8;
            blockBitmap.byte[j] ^= (1 << (7 - k));
        }
        else
            return -1;
    }
    superBlock->availBlockNum += inode->blockCount;
    diskWrite((void *)&blockBitmap, sizeof(BlockBitmap), 1, blockBitmapOffset * SECTOR_SIZE);

    //change father's DirEntry
    int inodeIndex = (inodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) + 1;
    int i_dir = 0;
    int j_dir = 0;
    int ret = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];

    for (i_dir = 0; i_dir < fatherinode->blockCount; i_dir++)
    {
        ret = readBlock(superBlock, fatherinode, i_dir, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j_dir = 0; j_dir < superBlock->blockSize / sizeof(DirEntry); j_dir++)
        {
            if (dirEntry[j_dir].inode == inodeIndex)
                break;
        }
        if (dirEntry[j_dir].inode == inodeIndex)
            break;
    }
    dirEntry[j_dir].inode = 0;
    ret = writeBlock(superBlock, fatherinode, i_dir, buffer);
    if (ret == -1)
        return -1;
    diskWrite((void *)superBlock, sizeof(SuperBlock), 1, 0);
    return 0;
}

int rm(const char *destFilePath)
{
    char tmp = 0;
    int length = 0;
    int ret = 0;
    int size = 0;
    SuperBlock superBlock;
    int fatherInodeOffset = 0;
    int inodeOffset = 0;
    Inode fatherInode;
    Inode inode;
    ret = readSuperBlock(&superBlock);
    if (ret == -1)
    {
        putString("Failed to load SuperBlock.\n");
        return -1;
    }
    if (destFilePath == NULL)
    {
        putString("destFilePath == NULL");
        return -1;
    }
    ret = readInode(&superBlock, &inode, &inodeOffset, destFilePath);
    if (ret == -1)
    {
        putString("Failed to read inode.\n");
        return -1;
    }
    if (inode.type == DIRECTORY_TYPE)
    {
        return rmdir(destFilePath);
    }
    length = stringLen(destFilePath);
    if (destFilePath[0] != '/' || destFilePath[length - 1] == '/')
    {
        putString("Incorrect destination file path.\n");
        return -1;
    }
    ret = stringChrR(destFilePath, '/', &size);
    if (ret == -1)
    { // no '/' in destFilePath
        putString("Incorrect destination file path.\n");
        return -1;
    }
    tmp = *((char *)destFilePath + size + 1);
    *((char *)destFilePath + size + 1) = 0; // destFilePath is dir ended with '/'.
    ret = readInode(&superBlock, &fatherInode, &fatherInodeOffset, destFilePath);
    *((char *)destFilePath + size + 1) = tmp;
    if (ret == -1)
    {
        putString("Failed to read father inode.\n");
        return -1;
    }
    ret = myremove(&inode, &fatherInode, &superBlock, fatherInodeOffset, inodeOffset);
    if (ret == -1)
    {
        putString("Failed to remove.\n");
        return -1;
    }
    return 0;
}

int rmdir(const char *destDirPath)
{
    int ret = 0;
    SuperBlock superBlock;
    Inode inode;
    int inodeOffset = 0;
    DirEntry dirEntry;
    int dirIndex = 0;

    if (destDirPath == NULL)
    {
        putString("destDirPath == NULL.\n");
        return -1;
    }
    ret = readSuperBlock(&superBlock);
    if (ret == -1)
    {
        putString("Failed to load superBlock.\n");
        return -1;
    }
    ret = readInode(&superBlock, &inode, &inodeOffset, destDirPath);
    if (ret == -1)
    {
        putString("Failed to read inode.\n");
        return -1;
    }
    //delete files in child
    while (getDirEntry(&superBlock, &inode, dirIndex, &dirEntry) == 0)
    {
        dirIndex++;
        if (dirEntry.name[0] == '.')
            continue;
        char childFilePath[NAME_LENGTH];
        stringCpy(destDirPath, childFilePath, NAME_LENGTH);
        int length = stringLen(childFilePath);
        childFilePath[length] = '/';
        stringCpy(dirEntry.name, childFilePath + length + 1, NAME_LENGTH - length - 1);
        ret = rm(childFilePath);
        if (ret == -1)
        {
            putString("Failed to rm");
            return -1;
        }
    }
    ret = rm(destDirPath);
    if (ret == -1)
    {
        putString("Failed to rmdir");
        return -1;
    }
    return 0;
}

int calNeededPointerBlocks(SuperBlock *superBlock, int blockCount)
{
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    if (blockCount == bound0)
        return 1;
    else if (blockCount >= bound1)
        return -1;
    else
        return 0;
}

int getAvailBlock(SuperBlock *superBlock, int *blockOffset)
{
    int j = 0;
    int k = 0;
    int blockBitmapOffset = 0;
    BlockBitmap blockBitmap;

    if (superBlock->availBlockNum == 0)
        return -1;
    superBlock->availBlockNum--;

    blockBitmapOffset = superBlock->blockBitmap;
    diskRead((void *)&blockBitmap, sizeof(BlockBitmap), 1, blockBitmapOffset * SECTOR_SIZE);
    for (j = 0; j < superBlock->blockNum / 8; j++)
    {
        if (blockBitmap.byte[j] != 0xff)
        {
            break;
        }
    }
    for (k = 0; k < 8; k++)
    {
        if ((blockBitmap.byte[j] >> (7 - k)) % 2 == 0)
        {
            break;
        }
    }
    blockBitmap.byte[j] = blockBitmap.byte[j] | (1 << (7 - k));

    *blockOffset = superBlock->blocks + ((j * 8 + k) * superBlock->blockSize / SECTOR_SIZE);

    diskWrite((void *)superBlock, sizeof(SuperBlock), 1, 0);
    diskWrite((void *)&blockBitmap, sizeof(BlockBitmap), 1, blockBitmapOffset * SECTOR_SIZE);

    return 0;
}

int allocLastBlock(SuperBlock *superBlock, Inode *inode, int inodeOffset, int blockOffset)
{
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];
    int singlyPointerBufferOffset = 0;

    if (inode->blockCount < bound0)
    {
        inode->pointer[inode->blockCount] = blockOffset;
    }
    else if (inode->blockCount == bound0)
    {
        getAvailBlock(superBlock, &singlyPointerBufferOffset);
        singlyPointerBuffer[0] = blockOffset;
        inode->singlyPointer = singlyPointerBufferOffset;
        diskWrite((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, singlyPointerBufferOffset * SECTOR_SIZE);
    }
    else if (inode->blockCount < bound1)
    {
        diskRead((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
        singlyPointerBuffer[inode->blockCount - bound0] = blockOffset;
        diskWrite((void *)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
    }
    else
        return -1;

    inode->blockCount++;
    diskWrite((void *)inode, sizeof(Inode), 1, inodeOffset);
    return 0;
}

int getAvailInode(SuperBlock *superBlock, int *inodeOffset)
{
    int j = 0;
    int k = 0;
    int inodeBitmapOffset = 0;
    int inodeTableOffset = 0;
    InodeBitmap inodeBitmap;

    if (superBlock->availInodeNum == 0)
        return -1;
    superBlock->availInodeNum--;

    inodeBitmapOffset = superBlock->inodeBitmap;
    inodeTableOffset = superBlock->inodeTable;
    diskRead((void *)&inodeBitmap, sizeof(InodeBitmap), 1, inodeBitmapOffset * SECTOR_SIZE);
    for (j = 0; j < superBlock->availInodeNum / 8; j++)
    {
        if (inodeBitmap.byte[j] != 0xff)
        {
            break;
        }
    }
    for (k = 0; k < 8; k++)
    {
        if ((inodeBitmap.byte[j] >> (7 - k)) % 2 == 0)
        {
            break;
        }
    }
    inodeBitmap.byte[j] = inodeBitmap.byte[j] | (1 << (7 - k));

    *inodeOffset = inodeTableOffset * SECTOR_SIZE + (j * 8 + k) * sizeof(Inode);

    diskWrite((void *)superBlock, sizeof(SuperBlock), 1, 0);
    diskWrite((void *)&inodeBitmap, sizeof(InodeBitmap), 1, inodeBitmapOffset * SECTOR_SIZE);

    return 0;
}
