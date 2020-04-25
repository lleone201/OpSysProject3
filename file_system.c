//Written by Logan Leone and Aaron Cohen
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>

#define MAX 1000000
// vars for getting instructions
typedef struct
{
    char **tokens;
    int numTokens;
} instruction;

struct open_file
{
    char file_name[12];
    int32_t address;
    uint32_t size;
    char mode[3];
};

int count;
bool opened_file = false;

//Fat variables
char BS_OEMName[8];      // the name of the OEM
int16_t BPB_BytesPerSec; // bytes per sector
int8_t BPB_SecPerClus;   // sectors per cluster
int16_t BPB_RsvdSecCnt;  // number of reserved sectors in reserved region
int8_t BPB_NumFATs;      // number of FAT data structures (should be 2)
int16_t BPB_RootEntCnt;  // number of 32 byte directories in the root (should be 0)
char BS_VolLab[11];      // label of the volume
int32_t BPB_FATSz32;     // number of sectors contained in one FAT
int32_t BPB_RootClus;    // the number of the first cluster of the root directory
int32_t RootDirCluster = 0;
int32_t CurrentDirCluster = 0;
FILE *fat_file;
FILE *read_fat;

struct DirEntry
{
    char Dir_Name[12];
    //int endCluster;
    uint8_t Dir_Attr;
    uint32_t Dir_FileSize;
    uint16_t Dir_FirstClusterLow;
    uint16_t Dir_FirstClusterHigh;
    uint8_t unused[8];
    uint8_t unused2[4];
};

struct DirEntry DIR[16]; //Entries in the current directory
struct open_file file_to_open;
struct open_file opened_file_array[20]; //Table to keep open files and their addresses in it.
int curr_open_files = 0;

void mount_image(char *file);                                 // Done Working
void populate_directory(int Directory, struct DirEntry *dir); // Done Working
void addToken(instruction *instr_ptr, char *tok);             //Done Working
void printTokens(instruction *instr_ptr);                     //Done Working
void clearInstruction(instruction *instr_ptr);                //Done Working
void performInstruction(instruction *instr_ptr);              // Just add functions to it
void addNull(instruction *instr_ptr);                         //Done Working
void info(instruction *instr_ptr);                            //Done working Aaron
void size(instruction *instr_ptr, struct DirEntry *dir);      //Done Working Logan May need quick tweak
void ls(instruction *instr_ptr, struct DirEntry *dir);        //Done Working Logan
void cd(instruction *instr_ptr, struct DirEntry *dir);        //Done Working Logan
void creatFile(instruction *instr_ptr, struct DirEntry *dir); //Working on Logan
void mkDirectory(instruction *instr_ptr, struct DirEntry *dir);
void mv(instruction *instr_ptr);
void openFile(instruction *instr_ptr, struct DirEntry *dir); //Done Working Logan
void closeFile(instruction *instr_ptr);                      //Done Working Logan
void readFile(instruction *instr_ptr);
void writeFile(instruction *instr_ptr);
void rm(instruction *instr_ptr);
void cp(instruction *instr_ptr);
int BlocktoAddress(int32_t sector);

void toUpperWSpaces(char *directory_name);
int16_t nextSector(int16_t sector);
void printOpenTable(); //Used to see what files are in the open table list

int main()
{
    char *token = NULL;
    char *temp = NULL;

    instruction instr;
    instr.tokens = NULL;
    instr.numTokens = 0;
    bool cont = true;
    mount_image("aaronfat32.img");
    do
    {
        printf("$ ");

        // loop reads character sequences separated by whitespace
        do
        {
            //scans for next token and allocates token var to size of scanned token
            scanf("%ms", &token);
            temp = (char *)malloc((strlen(token) + 1) * sizeof(char));

            int i;
            int start = 0;
            for (i = 0; i < strlen(token); i++)
            {
                //pull out special characters and make them into a separate token in the instruction
                if (token[i] == '|' || token[i] == '>' || token[i] == '<' || token[i] == '&')
                {
                    if (i - start > 0)
                    {
                        memcpy(temp, token + start, i - start);
                        temp[i - start] = '\0';
                        addToken(&instr, temp);
                    }

                    char specialChar[2];
                    specialChar[0] = token[i];
                    specialChar[1] = '\0';

                    addToken(&instr, specialChar);

                    start = i + 1;
                }
            }

            if (start < strlen(token))
            {
                memcpy(temp, token + start, strlen(token) - start);
                temp[i - start] = '\0';
                addToken(&instr, temp);
            }

            //free and reset variables
            free(token);
            free(temp);

            token = NULL;
            temp = NULL;
        } while ('\n' != getchar()); //until end of line is reached

        addNull(&instr);
        performInstruction(&instr);
        clearInstruction(&instr);
    } while (cont = true);

    return 0;
}

//reallocates instruction array to hold another token
//allocates for new token within instruction array
void addToken(instruction *instr_ptr, char *tok)
{
    //extend token array to accomodate an additional token
    if (instr_ptr->numTokens == 0)
        instr_ptr->tokens = (char **)malloc(sizeof(char *));
    else
        instr_ptr->tokens = (char **)realloc(instr_ptr->tokens, (instr_ptr->numTokens + 1) * sizeof(char *));

    //allocate char array for new token in new slot
    instr_ptr->tokens[instr_ptr->numTokens] = (char *)malloc((strlen(tok) + 1) * sizeof(char));
    strcpy(instr_ptr->tokens[instr_ptr->numTokens], tok);

    instr_ptr->numTokens++;
}

void addNull(instruction *instr_ptr)
{
    //extend token array to accomodate an additional token
    if (instr_ptr->numTokens == 0)
        instr_ptr->tokens = (char **)malloc(sizeof(char *));
    else
        instr_ptr->tokens = (char **)realloc(instr_ptr->tokens, (instr_ptr->numTokens + 1) * sizeof(char *));

    instr_ptr->tokens[instr_ptr->numTokens] = (char *)NULL;
    instr_ptr->numTokens++;
}

void printTokens(instruction *instr_ptr)
{
    int i;
    printf("Tokens:\n");
    for (i = 0; i < instr_ptr->numTokens; i++)
    {
        if ((instr_ptr->tokens)[i] != NULL)
            printf("%s\n", (instr_ptr->tokens)[i]);
    }
}

void clearInstruction(instruction *instr_ptr)
{
    int i;
    for (i = 0; i < instr_ptr->numTokens; i++)
        free(instr_ptr->tokens[i]);

    free(instr_ptr->tokens);

    instr_ptr->tokens = NULL;
    instr_ptr->numTokens = 0;
}
//*********************************************************< performInstruction >************************************************************************************
void performInstruction(instruction *instr_ptr)
{

    if (instr_ptr->numTokens != 0)
    {
        if (strcmp(instr_ptr->tokens[0], "exit") == 0)
        {
            exit(0);
        }

        if (strcmp(instr_ptr->tokens[0], "info") == 0)
        {
            info(instr_ptr);
        }
        else if (strcmp(instr_ptr->tokens[0], "ls") == 0)
        {
            ls(instr_ptr, DIR);
        }
        else if (strcmp(instr_ptr->tokens[0], "size") == 0)
        {
            size(instr_ptr, DIR);
        }
        else if (strcmp(instr_ptr->tokens[0], "cd") == 0)
        {
            cd(instr_ptr, DIR);
        }
        else if (strcmp(instr_ptr->tokens[0], "creat") == 0)
        {
            creatFile(instr_ptr, DIR);
        }
        else if (strcmp(instr_ptr->tokens[0], "mkdir") == 0)
        {
            mkDirectory(instr_ptr, DIR);
        }
        else if (strcmp(instr_ptr->tokens[0], "mv") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "open") == 0)
        {
            openFile(instr_ptr, DIR);
        }
        else if (strcmp(instr_ptr->tokens[0], "close") == 0)
        {
            closeFile(instr_ptr);
        }
        else if (strcmp(instr_ptr->tokens[0], "read") == 0)
        {
            readFile(instr_ptr);
        }
        else if (strcmp(instr_ptr->tokens[0], "write") == 0)
        {
            writeFile(instr_ptr);
        }
        else if (strcmp(instr_ptr->tokens[0], "rm") == 0)
        {
            rm(instr_ptr);
        }
        else if (strcmp(instr_ptr->tokens[0], "cp") == 0)
        {
            cp(instr_ptr);
        }
        else if (strcmp(instr_ptr->tokens[0], "print") == 0)
        {
            printOpenTable();
        }
        else
        {
            //Print error because that command does not exist
            printf("%s: Instruction is not recognized\n", instr_ptr->tokens[0]);
        }
    }
}

//**************************************************************< mount_image >************************************************************************************
void mount_image(char *file)
{
    fat_file = fopen(file, "r+");
    if (fat_file != NULL)
    {
        printf("Opened file: %s\n", file);
        opened_file = true;

        //fseek sets the position of the stream
        //SEEK_SET marks the beginning of the file
        fseek(fat_file, 3, SEEK_SET);
        fread(BS_OEMName, 1, 8, fat_file);
        fread(&BPB_BytesPerSec, 1, 2, fat_file);
        fread(&BPB_SecPerClus, 1, 1, fat_file);
        fread(&BPB_RsvdSecCnt, 1, 2, fat_file);
        fread(&BPB_NumFATs, 1, 1, fat_file);
        fread(&BPB_RootEntCnt, 1, 2, fat_file);

        fseek(fat_file, 36, SEEK_SET);
        fread(&BPB_FATSz32, 1, 4, fat_file);
        fseek(fat_file, 44, SEEK_SET);
        fread(&BPB_RootClus, 1, 4, fat_file);
        fseek(fat_file, 71, SEEK_SET);
        fread(BS_VolLab, 1, 11, fat_file);

        //Root Directory calculation
        RootDirCluster = (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec) + (BPB_RsvdSecCnt * BPB_BytesPerSec);

        //Start point
        CurrentDirCluster = RootDirCluster;

        populate_directory(CurrentDirCluster, DIR);
    }
    else
    {
        printf("Couldn't open the file:  %s\n", file);
    }
}

//*********************************************************< populate_directory >************************************************************************************
void populate_directory(int Directory, struct DirEntry *dir)
{
    fseek(fat_file, Directory, SEEK_SET);
    int i;
    for (i = 0; i < 16; i++)
    {
        fread(dir[i].Dir_Name, 1, 11, fat_file);
        dir[i].Dir_Name[11] = 0;
        fread(&dir[i].Dir_Attr, 1, 1, fat_file);
        fread(&dir[i].unused, 1, 8, fat_file);
        fread(&dir[i].Dir_FirstClusterHigh, 2, 1, fat_file);
        fread(&dir[i].unused2, 1, 4, fat_file);
        fread(&dir[i].Dir_FirstClusterLow, 2, 1, fat_file);
        fread(&dir[i].Dir_FileSize, 4, 1, fat_file);
    }
}

//*********************************************************************< info >************************************************************************************
void info(instruction *instr_ptr)
{
    printf("Bytes Per Sector: %d\n", BPB_BytesPerSec);
    printf("Sectors Per Cluster: %d\n", BPB_SecPerClus);
    printf("Reserved Sector Count: %d\n", BPB_RsvdSecCnt);
    printf("Number of FATS: %d\n", BPB_NumFATs);
    printf("Total Sectors: %d\n", BPB_FATSz32);
    printf("FAT size: \n");
    printf("Root Cluster: %d\n", BPB_RootClus);
}

//*********************************************************************< size >************************************************************************************
void size(instruction *instr_ptr, struct DirEntry *dir)
{

    populate_directory(CurrentDirCluster, dir);
    bool found = false;
    //Check the current directory for the file name
    //printf("Into the directory\n");
    if (instr_ptr->tokens[1] != NULL)
    {
        char instr[15];
        strcpy(instr, instr_ptr->tokens[1]);
        toUpperWSpaces(instr);
        //printf("Changed name");
        for (count = 0; count < 16; count++)
        {
            //If the file is in the directory print out the size
            if (strcmp(dir[count].Dir_Name, instr) == 0)
            {
                printf("%s    %d Bytes      unused1: %d     unused2: %d\n", dir[count].Dir_Name, dir[count].Dir_FileSize, dir[count].unused, dir[count].unused2);
                found = true;
            }
        }

        if (found == false)
        {
            printf("Cannot find that file in the directory\n");
        }
        return;
    }
    else
    {
        printf("Usage: size <filename>\n");
    }
}
//*********************************************************************< ls >************************************************************************************
void ls(instruction *instr_ptr, struct DirEntry *dir)
{
    //Starting with the same code that I used for size because I looped through the directory to do that.
    char tempname[15];                    //token to hold the directory name
    int32_t tempAddr = CurrentDirCluster; //this will be used to hold the location of the directory we are in
    struct DirEntry tempdir[16];          //this will be used to hold the location of the directory we are in
    bool found = false;
    int16_t sector;
    bool breakLoop = false;
    char *tokens; //To split the arguments into
    //Print out current directory contents
    if (instr_ptr->tokens[1] == NULL || strcmp(instr_ptr->tokens[1], ".") == 0)
    {
        //The current directory address
        populate_directory(tempAddr, tempdir);
        sector = tempdir[0].Dir_FirstClusterLow;
        int i;
        do
        {
            for (i = 0; i < 16; i++)
            {
                if (tempdir[i].Dir_Attr == 32 || tempdir[i].Dir_Attr == 16 || tempdir[i].Dir_Attr == 2 || tempdir[i].Dir_Attr == 1 || tempdir[i].Dir_Attr == 0)
                {
                    printf("%s\n", tempdir[i].Dir_Name);
                }
                if (strcmp(tempdir[i].Dir_Name, "") == 0)
                {
                    breakLoop = true;
                    break;
                }
            }

            //Get address of the next sector and populate the temp directory with that so I can loop through that until I get to the end of the cluster
            sector = nextSector(sector);
            tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
            populate_directory(tempAddr, tempdir);
        } while (breakLoop == false);
    }
    else if (instr_ptr->tokens[1] != NULL)
    {
        //Means they have a parameter specifying a directory.
        //Split the token on the slashes
        tokens = strtok(instr_ptr->tokens[1], "/");

        while (tokens != NULL)
        {
            strcpy(tempname, tokens);
            toUpperWSpaces(tempname);
            populate_directory(tempAddr, tempdir);
            found = false;
            int i;
            for (i = 0; i < 16; i++)
            {
                if (strcmp(tempdir[i].Dir_Name, tempname) == 0)
                {
                    //If the name isn't there, get the address of the next sector and put that sector into tempdir
                    sector = tempdir[i].Dir_FirstClusterLow;
                    tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((tempdir[i].Dir_FirstClusterLow - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
                    populate_directory(tempAddr, tempdir);
                    found = true;
                    break;
                }
            }
            if (found == false)
            {
                printf("Directory doesn't exist\n");
                return;
            }

            //If one directory is found, but there is more after it, move on to the next directory down
            tokens = strtok(NULL, "/");

            //Once we get to the end of the directory path, list the contents of the directory.
            if (tokens == NULL)
            {
                int i;
                do
                {
                    for (i = 0; i < 16; i++)
                    {
                        if (tempdir[i].Dir_Attr == 32 || tempdir[i].Dir_Attr == 16 || tempdir[i].Dir_Attr == 2 || tempdir[i].Dir_Attr == 1 || tempdir[i].Dir_Attr == 0)

                        {
                            printf("%s\n", tempdir[i].Dir_Name);
                        }
                        if (strcmp(tempdir[i].Dir_Name, "") == 0)
                        {
                            breakLoop = true;
                            break;
                        }
                    }

                    //Get address of the next sector and populate the temp directory with that so I can loop through that until I get to the end of the cluster
                    sector = nextSector(sector);
                    tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
                    populate_directory(tempAddr, tempdir);
                } while (breakLoop == false);
            }
        }
    }
}

//*********************************************************************< cd >************************************************************************************
void cd(instruction *instr_ptr, struct DirEntry *dir)
{
    char tempname[15]; //token to hold the directory name
    int32_t tempAddr = CurrentDirCluster;
    struct DirEntry tempdir[16]; //this will be used to hold the location of the directory we are in
    struct DirEntry prevdir[16];
    bool found = false;
    int16_t sector;
    bool breakLoop = false;

    //Check to make sure the command is being used correctly
    if (instr_ptr->tokens[2] != NULL)
    {
        printf("Usage: cd <directory_name>\n");
        return;
    }

    //Goes back to the root cluster if there is no directory name
    if (instr_ptr->tokens[1] == NULL)
    {
        CurrentDirCluster = RootDirCluster;
        populate_directory(CurrentDirCluster, dir);
        return;
    }

    //Use same method of going through the directories as ls
    //Split the directory path on slashes to look for each directory individually
    if (strcmp(instr_ptr->tokens[1], ".") == 0)
    {
        //cd to the current directory, so do nothing
        return;
    }

    strcpy(tempname, instr_ptr->tokens[1]);
    toUpperWSpaces(tempname);
    populate_directory(tempAddr, tempdir);
    found = false;
    int i;
    do
    {
        for (i = 0; i < 16; i++)
        {
            if (strcmp(tempdir[i].Dir_Name, tempname) == 0)
            {
                //If the name isn't there, get the address of the next sector and put that sector into tempdir
                if (tempdir[i].Dir_Attr == 32)
                {
                    printf("Cannot cd to a file\n");
                    return;
                }
                sector = tempdir[i].Dir_FirstClusterLow;
                tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((tempdir[i].Dir_FirstClusterLow - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
                populate_directory(tempAddr, tempdir);
                found = true;
                break;
            }

            if (strcmp(tempdir[i].Dir_Name, "") == 0)
            {
                breakLoop = true;
                break;
            }
        }
        //If the file has been found, don't go to the next sector before exiting this loop
        if (found == true)
        {
            break;
        }
        //Get address of the next sector and populate the temp directory with that so I can loop through that until I get to the end of the cluster
        sector = nextSector(sector);
        tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
        populate_directory(tempAddr, tempdir);
    } while (found == false && breakLoop == false);

    if (found == false)
    {
        printf("Directory doesn't exist\n");
        return;
    }
    else
    {
        CurrentDirCluster = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
        populate_directory(CurrentDirCluster, dir);
        return;
    }
}

//************************************************************< openFile >************************************************************************************
void openFile(instruction *instr_ptr, struct DirEntry *dir)
{
    char tempname[15];                    //token to hold the directory name
    int32_t tempAddr = CurrentDirCluster; //this will be used to hold the location of the directory we are in
    struct DirEntry tempdir[16];          //this will be used to hold the location of the directory we are in
    bool found = false;
    int16_t sector;
    uint32_t size;
    char name[15];
    bool breakLoop = false;
    bool modeFlag = false;

    //Check for the correct usage
    if (instr_ptr->numTokens != 4)
    {
        printf("Usage: openFile <filename> <mode>\n");
        return;
    }

    //Check if mode parameter is valid
    if ((strcmp(instr_ptr->tokens[2], "r") == 0) || (strcmp(instr_ptr->tokens[2], "w") == 0) || strcmp(instr_ptr->tokens[2], "wr") == 0 || strcmp(instr_ptr->tokens[2], "rw") == 0)
    {
        modeFlag = true;
    }
    if (modeFlag == false)
    {
        printf("Mode must be r | w | rw | wr\n");
        return;
    }

    strcpy(tempname, instr_ptr->tokens[1]);
    toUpperWSpaces(tempname);
    //Check if the file is already open
    for (count = 0; count < curr_open_files; count++)
    {
        if (strcmp(tempname, opened_file_array[count].file_name) == 0)
        {
            printf("This file is already open\n");
            return;
        }
    }

    populate_directory(tempAddr, tempdir);

    int i;
    do
    {
        for (i = 0; i < 16; i++)
        {
            if (strcmp(tempdir[i].Dir_Name, tempname) == 0)
            {
                //If the name isn't there, get the address of the next sector and put that sector into tempdir
                if (tempdir[i].Dir_Attr == 16)
                {
                    printf("Can only open files, not directories.\n");
                    return;
                }
                sector = tempdir[i].Dir_FirstClusterLow;
                size = tempdir[i].Dir_FileSize;
                strcpy(name, tempdir[i].Dir_Name);
                tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((tempdir[i].Dir_FirstClusterLow - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
                populate_directory(tempAddr, tempdir);
                found = true;
                break;
            }

            if (strcmp(tempdir[i].Dir_Name, "") == 0)
            {
                breakLoop = true;
                printf("BREAKLOOP\n");
                break;
            }
        }
        //If the file has been found, don't go to the next sector before exiting this loop
        if (found == true)
        {
            break;
        }
        //Get address of the next sector and populate the temp directory with that so I can loop through that until I get to the end of the cluster
        sector = nextSector(sector);
        tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
        populate_directory(tempAddr, tempdir);
    } while (found == false && breakLoop == false);

    if (found == false)
    {
        printf("File doesn't exist\n");
        return;
    }
    else
    {
        struct open_file o1;
        strcpy(o1.file_name, name);
        o1.size = size;
        o1.address = sector;
        strcpy(o1.mode, instr_ptr->tokens[2]);
        opened_file_array[curr_open_files] = o1;
        curr_open_files++;
        printf("File %s opened\n", instr_ptr->tokens[1]);
        return;
    }
}

//************************************************************< closeFile >************************************************************************************
void closeFile(instruction *instr_ptr)
{
    char tempname[15]; //token to hold the directory name
    bool fileOpened = false;
    //Check parameters of the instruction
    if (instr_ptr->tokens[1] == NULL)
    {
        printf("Usage: close <filename>\n");
        return;
    }

    strcpy(tempname, instr_ptr->tokens[1]);
    toUpperWSpaces(tempname);
    //Check to see if the file is open so that it can be closed
    for (count = 0; count < curr_open_files; count++)
    {
        if (strcmp(tempname, opened_file_array[count].file_name) == 0)
        {

            fileOpened = true;
            //Break so that I can use count as the index to delete from the table
            break;
        }
    }

    //If the file isn't opened print out error
    if (fileOpened == false)
    {
        printf("File is not open or doesn't exist\n");
        return;
    }
    else
    {
        if (curr_open_files == 1)
        {
            //opened_file_array[0].file_name = "";
            curr_open_files--;
            printf("File %s closed\n", instr_ptr->tokens[1]);
            return;
        }
        else
        {
            //Remove the file from the open table, effectively closing it
            int i;
            for (i = count - 1; i < curr_open_files - 1; i++)
            {
                opened_file_array[i] = opened_file_array[i + 1];
            }
            //printf("File %s closed\n", instr_ptr->tokens[1]);
            curr_open_files--;
            return;
        }
    }
}

//************************************************************< creatFile >************************************************************************************
void creatFile(instruction *instr_ptr, struct DirEntry *dir)
{
    //Starting with the same code that I used for size because I looped through the directory to do that.
    char tempname[15];                    //this will be used to hold the name of the directory after changing it's form
    int32_t tempAddr = CurrentDirCluster; //this will be used to hold the location of the directory we are in
    struct DirEntry tempdir[16];          //this will be used to hold the contents of the directory we are in
    bool found = false;                   //boolean for when we find the file
    bool breakLoop = false;               //boolean to know when we have hit the end of the directory and can break out
    int16_t sector;                       //this will hold the value of the next sector to go to
    //These are the variables that will be written to create the file
    char Dir_Name[12];
    char attr[1];
    char size[4];
    char FirstClusterLow[2];
    char FirstClusterHigh[2];
    uint8_t myunused[8];
    uint8_t myunused2[4];

    //Argument checking
    if (instr_ptr->tokens[1] == NULL)
    {
        printf("Usage: creat <filename>\n");
        return;
    }

    //Changes the directory/filename to fat32 format
    strcpy(tempname, instr_ptr->tokens[1]);
    toUpperWSpaces(tempname);

    populate_directory(tempAddr, tempdir);
    found = false;
    int i;
    do
    {
        for (i = 0; i < 16; i++)
        {
            if (strcmp(tempdir[i].Dir_Name, tempname) == 0 && tempdir[i].Dir_Attr == 32)
            {
                //If file name is found in directory print that it already exists
                printf("That file already exists\n");
                return;
            }

            if (strcmp(tempdir[i].Dir_Name, "") == 0)
            {
                //Takes the ending address of the directory for later
                breakLoop = true;
                sector = tempdir[i].Dir_FirstClusterLow;
                tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((tempdir[i].Dir_FirstClusterLow - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
                populate_directory(tempAddr, tempdir);
                found = true;
                break;
            }
        }
        //If the file has been found, don't go to the next sector before exiting this loop
        if (found == true)
        {
            //If end of fule is found we can take that address and stop searching.
            break;
        }

        //Get address of the next sector and populate the temp directory with that so I can loop through that until I get to the end of the cluster
        sector = nextSector(sector);
        tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
        populate_directory(tempAddr, tempdir);
    } while (found == false && breakLoop == false);

    //Sector should point to open spot. so we create the new file there
    if (found == true)
    {
        //Set all variables and write them to the end of the directory
        myunused[0] = tempdir[0].unused[0];
        myunused[1] = tempdir[0].unused[1];
        myunused[2] = tempdir[0].unused[2];
        myunused[3] = tempdir[0].unused[3];
        myunused[4] = tempdir[0].unused[4];
        myunused[5] = tempdir[0].unused[5];
        myunused[6] = tempdir[0].unused[6];
        myunused[7] = tempdir[0].unused[7];
        myunused2[0] = tempdir[0].unused2[0];
        myunused2[1] = tempdir[0].unused2[1];
        myunused2[2] = tempdir[0].unused2[2];
        myunused2[3] = tempdir[0].unused2[3];
        snprintf(size, sizeof(size), "%d", 0);
        snprintf(attr, sizeof(attr), "%d", 32);
        snprintf(FirstClusterHigh, sizeof(FirstClusterHigh), "%d", tempdir[0].Dir_FirstClusterHigh);
        snprintf(FirstClusterLow, sizeof(FirstClusterLow), "%d", tempdir[0].Dir_FirstClusterLow);

        fseek(fat_file, tempAddr, SEEK_SET);
        fwrite(tempname, 1, 11, fat_file);
        fwrite(attr, 1, 1, fat_file);
        fwrite(myunused, 1, 8, fat_file);
        fwrite(FirstClusterHigh, 2, 1, fat_file);
        fwrite(myunused2, 1, 4, fat_file);
        fwrite(FirstClusterLow, 2, 1, fat_file);
        fwrite(size, 4, 1, fat_file);
        printf("File: %s created\n", tempname);
        return;
    }
}

//************************************************************< mkDirectory >************************************************************************************
void mkDirectory(instruction *instr_ptr, struct DirEntry *dir)
{
    //Starting with the same code that I used for size because I looped through the directory to do that.
    char tempname[15];                    //this will be used to hold the name of the directory after changing it's form
    int32_t tempAddr = CurrentDirCluster; //this will be used to hold the location of the directory we are in
    struct DirEntry tempdir[16];          //this will be used to hold the contents of the directory we are in
    bool found = false;                   //boolean for when we find the file
    bool breakLoop = false;               //boolean to know when we have hit the end of the directory and can break out
    int16_t sector;                       //this will hold the value of the next sector to go to
    //These are the variables that will be written to create the file
    char Dir_Name[12];
    char attr[1];
    char size[4];
    char FirstClusterLow[2];
    char FirstClusterHigh[2];
    uint8_t myunused[8];
    uint8_t myunused2[4];

    //Argument checking
    if (instr_ptr->tokens[1] == NULL)
    {
        printf("Usage: mkdir <dirname>\n");
        return;
    }

    //Changes the directory/filename to fat32 format
    strcpy(tempname, instr_ptr->tokens[1]);
    toUpperWSpaces(tempname);

    populate_directory(tempAddr, tempdir);
    found = false;
    int i;
    do
    {
        for (i = 0; i < 16; i++)
        {
            if (strcmp(tempdir[i].Dir_Name, tempname) == 0 && tempdir[i].Dir_Attr == 16)
            {
                //If file name is found in directory print that it already exists
                printf("That directory already exists\n");
                return;
            }

            if (strcmp(tempdir[i].Dir_Name, "") == 0)
            {
                //Takes the ending address of the directory for later
                breakLoop = true;
                sector = tempdir[i].Dir_FirstClusterLow;
                tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((tempdir[i].Dir_FirstClusterLow - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
                populate_directory(tempAddr, tempdir);
                found = true;
                break;
            }
        }
        //If the file has been found, don't go to the next sector before exiting this loop
        if (found == true)
        {
            //If end of fule is found we can take that address and stop searching.
            break;
        }

        //Get address of the next sector and populate the temp directory with that so I can loop through that until I get to the end of the cluster
        sector = nextSector(sector);
        tempAddr = (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
        populate_directory(tempAddr, tempdir);
    } while (found == false && breakLoop == false);

    //Sector should point to open spot. so we create the new file there
    if (found == true)
    {
        //Set all variables and write them to the end of the directory
        myunused[0] = tempdir[0].unused[0];
        myunused[1] = tempdir[0].unused[1];
        myunused[2] = tempdir[0].unused[2];
        myunused[3] = tempdir[0].unused[3];
        myunused[4] = tempdir[0].unused[4];
        myunused[5] = tempdir[0].unused[5];
        myunused[6] = tempdir[0].unused[6];
        myunused[7] = tempdir[0].unused[7];
        myunused2[0] = tempdir[0].unused2[0];
        myunused2[1] = tempdir[0].unused2[1];
        myunused2[2] = tempdir[0].unused2[2];
        myunused2[3] = tempdir[0].unused2[3];
        snprintf(size, sizeof(size), "%d", 0);
        snprintf(attr, sizeof(attr), "%d", 16);
        snprintf(FirstClusterHigh, sizeof(FirstClusterHigh), "%d", tempdir[0].Dir_FirstClusterHigh);
        snprintf(FirstClusterLow, sizeof(FirstClusterLow), "%d", tempdir[0].Dir_FirstClusterLow);

        fseek(fat_file, tempAddr, SEEK_SET);
        fwrite(tempname, 1, 11, fat_file);
        fwrite(attr, 1, 1, fat_file);
        fwrite(myunused, 1, 8, fat_file);
        fwrite(FirstClusterHigh, 2, 1, fat_file);
        fwrite(myunused2, 1, 4, fat_file);
        fwrite(FirstClusterLow, 2, 1, fat_file);
        fwrite(size, 4, 1, fat_file);
        printf("Directory: %s created\n", tempname);
        return;
    }
}

//********************************************************************<readFile>****************************************************************************************
void readFile(instruction *instr_ptr)
{
    //printf("%s %s\n", opened_file_array[0].file_name, opened_file_array[0].mode);
    char instr[15];
    strcpy(instr, instr_ptr->tokens[1]);
    toUpperWSpaces(instr);
    char tempname[15];
    //printf("%d\n", curr_open_files);
    int file;
    bool found = false;
    bool open = false;
    //check to see if the filesystem has been mounted
    if (opened_file == false)
    {
        printf("There is no filesystem open.\n");
        return;
    }
    //Check the current opened files for the file name
    //printf("%d\n", instr_ptr->numTokens);

    if (instr_ptr->numTokens == 5)
    {
        for (count = 0; count < curr_open_files; count++)
        {
            if (strcmp(instr, opened_file_array[count].file_name) == 0)
            {
                //If the file is found then check for the open type
                printf("File found\n");
                if (strcmp(opened_file_array[count].mode, "r") == 0 || strcmp(opened_file_array[count].mode, "rw") == 0 || strcmp(opened_file_array[count].mode, "wr") == 0)
                {
                    printf("Is opened\n");
                    open = true;
                }
                found = true;
            }
        }

        if (found == false)
        {
            //if it can't be found print error and return
            printf("Cannot find that file in the directory\n");
            return;
        }
        else if (open == false)
        {
            printf("File is not opened\n");
            return;
        }
    }
    else
    {
        printf("Usage: read <filename> <offset> <size>\n");
        return;
    }

    for (count = 0; count < 16; count++)
    {
        if (strcmp(instr, DIR[count].Dir_Name) == 0)
            file = count;
    }

    char buffer[513];
    int32_t tempCluster = CurrentDirCluster;
    int16_t Block;
    int32_t size;
    int data = atoi(instr_ptr->tokens[3]);
    int blockOffset = atoi(instr_ptr->tokens[2]) / 512;
    int byteOffset = atoi(instr_ptr->tokens[2]) % 512;
    int max;

    Block = DIR[file].Dir_FirstClusterLow;
    size = DIR[file].Dir_FileSize;

    if (size < (data + atoi(instr_ptr->tokens[2])))
    {
        printf("File is smaller than offset\n");
        return;
    }
    for (count = 0; count < blockOffset; count++)
    {
        Block = nextSector(Block);
    }

    tempCluster = BlocktoAddress(Block);
    fseek(fat_file, (tempCluster + byteOffset), SEEK_SET);

    while (1)
    {
        if (data < 512)
            max = data;
        else
            max = 512;

        fread(buffer, 1, 512, fat_file);
        buffer[max] = 0;

        for (count = 0; count < max; count++)
        {
            printf("%x ", buffer[count]);
        }

        data -= max;
        if (data == 0)
        {
            printf("\n");
            return;
        }

        Block = nextSector(Block);
        tempCluster = BlocktoAddress(Block);

        fseek(fat_file, tempCluster, SEEK_SET);
    }
}

//************************************************************************************************< writeFile >****************************************************************
void writeFile(instruction *instr_ptr)
{
    char instr[15];
    char write[30];
    strcpy(instr, instr_ptr->tokens[1]);
    toUpperWSpaces(instr);
    char tempname[15];
    int file;
    bool found = false;
    bool open = false;
    //check to see if the filesystem has been mounted
    if (opened_file == false)
    {
        printf("There is no filesystem open.\n");
        return;
    }
    strcpy(write, instr_ptr->tokens[4]);
    //Check the current opened files for the file name
    for (count = 5; count < (instr_ptr->numTokens - 1); count++)
    {
        strcat(write, " ");
        strcat(write, instr_ptr->tokens[count]);
    }

    if (instr_ptr->numTokens >= 5)
    {
        for (count = 0; count < curr_open_files; count++)
        {
            if (strcmp(instr, opened_file_array[count].file_name) == 0)
            {
                if (strcmp(opened_file_array[count].mode, "w") == 0 || strcmp(opened_file_array[count].mode, "rw") == 0 || strcmp(opened_file_array[count].mode, "wr") == 0)
                {
                    open = true;
                }
                found = true;
            }
        }

        if (found == false)
        {
            printf("Cannot find that file in the directory\n");
            return;
        }
        else if (open == false)
        {
            printf("File is not opened\n");
            return;
        }
    }
    else
    {
        printf("Usage: write <filename> <offset> <size> <string>\n");
        return;
    }

    for (count = 0; count < 16; count++)
    {
        if (strcmp(instr, DIR[count].Dir_Name) == 0)
            file = count;
    }
    char buffer[513];
    int32_t tempCluster = CurrentDirCluster;
    int16_t Block;
    int32_t size;
    int data = atoi(instr_ptr->tokens[3]);
    int blockOffset = atoi(instr_ptr->tokens[2]) / 512;
    int byteOffset = atoi(instr_ptr->tokens[2]) % 512;
    int max;

    Block = DIR[file].Dir_FirstClusterLow;
    size = DIR[file].Dir_FileSize;

    if (size < (data + atoi(instr_ptr->tokens[2])))
    {
        printf("File is smaller than offset\n");
        return;
    }
    for (count = 0; count < blockOffset; count++)
    {
        Block = nextSector(Block);
    }

    tempCluster = BlocktoAddress(Block);
    fseek(fat_file, (tempCluster + byteOffset), SEEK_SET);

    while (1)
    {
        if (data < 512)
            max = data;
        else
            max = 512;

        fwrite(write, 1, sizeof(write), fat_file);

        data -= max;
        if (data == 0)
        {
            return;
        }

        Block = nextSector(Block);
        tempCluster = BlocktoAddress(Block);

        fseek(fat_file, tempCluster, SEEK_SET);
    }
}

//************************************************************************************************< rm >****************************************************************
void rm(instruction *instr_ptr)
{
    int rmIndex = -1;
    toUpperWSpaces(instr_ptr->tokens[1]);
    if (instr_ptr->numTokens != 3)
    {
        printf("Usage: rm <filename>\n");
        return;
    }
    for (count = 0; count < 16; count++)
    {
        if (strcmp(instr_ptr->tokens[1], DIR[count].Dir_Name) == 0)
        {
            rmIndex = count;
        }
    }
    if (rmIndex == -1)
    {
        printf("File %s not found\n", instr_ptr->tokens[1]);
        return;
    }
    int max;
    char write[512];
    memset(write, '\0', sizeof(write));
    int32_t fileSize = DIR[rmIndex].Dir_FileSize;
    int16_t Block = DIR[rmIndex].Dir_FirstClusterLow;
    int32_t tempCluster;

    tempCluster = BlocktoAddress(Block);
    fseek(fat_file, tempCluster, SEEK_SET);
    while (1)
    {
        if (fileSize < 512)
            max = fileSize;
        else
            max = 512;
        fwrite(write, 1, 512, fat_file);
        fileSize -= max;
        if (fileSize == 0)
        {
            return;
        }
        Block = nextSector(Block);
        tempCluster = BlocktoAddress(Block);
    }
    struct DirEntry clear;
    strcpy(DIR[rmIndex].Dir_Name, "");
    DIR[rmIndex].Dir_Attr = clear.Dir_Attr;
    DIR[rmIndex].Dir_FileSize = clear.Dir_FileSize;
    DIR[rmIndex].Dir_FirstClusterLow = clear.Dir_FirstClusterLow;
    DIR[rmIndex].Dir_FirstClusterHigh = clear.Dir_FirstClusterHigh;
    fclose(fat_file);
    mount_image("aaronfat32.img");
}

//************************************************************************************************< cp >****************************************************************
void cp(instruction *instr_ptr)
{
    if (instr_ptr->numTokens != 4)
    {
        printf("Usage: cp <file> <destination>\n");
        return;
    }
    char tempname[15];
    bool found = false;
    int file = 0;
    int dirn = 0;
    strcpy(tempname, instr_ptr->tokens[1]);
    for (count = 0; count < 16; count++)
    {
        if (strcmp(tempname, DIR[count].Dir_Name) == 0)
        {
            found = true;
            file = count;
        }
    }
    if (!found)
    {
        printf("File %s not found\n", tempname);
        return;
    }

    char buffer[1024];
    int32_t tempCluster = CurrentDirCluster;
    int16_t Block;
    int32_t size;
    int max = 512;

    Block = DIR[file].Dir_FirstClusterLow;
    size = DIR[file].Dir_FileSize;

    tempCluster = BlocktoAddress(Block);
    fseek(fat_file, (tempCluster), SEEK_SET);

    while (1)
    {
        fread(buffer, 1, 512, fat_file);
        buffer[max] = 0;

        size -= max;
        if (size == 0)
        {
            printf("\n");
            return;
        }

        Block = nextSector(Block);
        tempCluster = BlocktoAddress(Block);

        fseek(fat_file, tempCluster, SEEK_SET);
    }
}

//************************************************************< printOpenTable >************************************************************************************
void printOpenTable()
{
    //Function to print out the names of files in the open table
    printf("Curr open files = %d\n", curr_open_files);

    int i;
    for (i = 0; i < curr_open_files; i++)
    {

        printf("%s\n", opened_file_array[i].file_name);
    }
}

//************************************************************< toUpperWSpaces >************************************************************************************
void toUpperWSpaces(char *directory_name)
{
    //Changes any directories or file names typed in to the correct format
    //Change all the letters to uppercase
    for (count = 0; count < 11; count++)
    {
        directory_name[count] = toupper(directory_name[count]);
    }
    //Add whitespacing at the end to meet standards
    int whitespace = 11 - strlen(directory_name);
    for (count = 0; count < whitespace; count++)
    {
        strcat(directory_name, " ");
    }
}

//*********************************************************************< nextSector >************************************************************************************
int16_t nextSector(int16_t sector)
{
    //Locates the next sector given the current secctor
    uint32_t FATAddr = (BPB_RsvdSecCnt * BPB_BytesPerSec) + (sector * 4);
    int16_t val;
    fseek(fat_file, FATAddr, SEEK_SET);
    fread(&val, 2, 1, fat_file);
    return val;
}

//********************************************************************< BlocktoAddress***************************************************************************
int BlocktoAddress(int32_t sector)
{
    if (!sector)
    {
        return RootDirCluster;
    }

    return (BPB_BytesPerSec * BPB_RsvdSecCnt) + ((sector - 2) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32);
}
