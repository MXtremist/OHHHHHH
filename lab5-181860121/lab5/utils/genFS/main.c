#include <stdio.h>
#include "utils.h"
#include "data.h"
#include "func.h"

int main(int argc, char *argv[])
{
    char driver[NAME_LENGTH];
    char srcFilePath[NAME_LENGTH];
    char destFilePath[NAME_LENGTH];

    stringCpy("fs.bin", driver, NAME_LENGTH - 1);
    format(driver, SECTOR_NUM, SECTORS_PER_BLOCK);

    stringCpy("/boot", destFilePath, NAME_LENGTH - 1);
    mkdir(driver, destFilePath);

    stringCpy(argv[1], srcFilePath, NAME_LENGTH - 1);
    stringCpy("/boot/initrd", destFilePath, NAME_LENGTH - 1);
    cp(driver, srcFilePath, destFilePath);

    stringCpy("/usr", destFilePath, NAME_LENGTH - 1);
    mkdir(driver, destFilePath);

    stringCpy(argv[2], srcFilePath, NAME_LENGTH - 1);
    stringCpy("/usr/print", destFilePath, NAME_LENGTH - 1);
    cp(driver, srcFilePath, destFilePath);

    //create /usr/testdir/
    stringCpy("/usr/testdir", destFilePath, NAME_LENGTH - 1);
    mkdir(driver, destFilePath);
    //put a file in /testdir
    stringCpy("/usr/testdir/test", destFilePath, NAME_LENGTH - 1);
    touch(driver, destFilePath);
    //put a file in /usr
    stringCpy("/usr/test2", destFilePath, NAME_LENGTH - 1);
    touch(driver, destFilePath);
    //ls /usr
    stringCpy("/usr", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);
    //delete file
    stringCpy("/usr/test2", destFilePath, NAME_LENGTH - 1);
    rm(driver, destFilePath, 0);
    //ls /usr
    stringCpy("/usr", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);
    //delete dir
    stringCpy("/usr/testdir", destFilePath, NAME_LENGTH - 1);
    rmdir(driver, destFilePath);
    //ls /usr
    stringCpy("/usr", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    stringCpy("/", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    stringCpy("/boot", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    stringCpy("/usr", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    return 0;
}
