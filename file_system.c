#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

typedef struct
{
    char **tokens;
    int numTokens;
} instruction;

void addToken(instruction *instr_ptr, char *tok);
void printTokens(instruction *instr_ptr);
void clearInstruction(instruction *instr_ptr);
void performInstruction(instruction *instr_ptr);
void addNull(instruction *instr_ptr);
void exitShell();
void info(instruction *instr_ptr);
void size(instruction *instr_ptr);
void ls(instruction *instr_ptr);
void cd(instruction *instr_ptr);
void creat(instruction *instr_ptr);
void mkdir(instruction *instr_ptr);
void mv(instruction *instr_ptr);
void openFile(instruction *instr_ptr);
void closeFile(instruction *instr_ptr);
void readFile(instruction *instr_ptr);
void writeFile(instruction *instr_ptr);
void rm(instruction *instr_ptr);
void cp(instruction *instr_ptr);

int main()
{
    char *token = NULL;
    char *temp = NULL;

    instruction instr;
    instr.tokens = NULL;
    instr.numTokens = 0;
    bool cont = true;

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
        //printTokens(&instr);
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

void performInstruction(instruction *instr_ptr)
{

    if (instr_ptr->numTokens != 0)
    {
        if (strcmp(instr_ptr->tokens[0], "exit") == 0)
        {
            //Exit the app
        }
        else if (strcmp(instr_ptr->tokens[0], "info") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "ls") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "size") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "cd") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "creat") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "mkdir") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "mv") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "open") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "close") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "read") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "write") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "rm") == 0)
        {
        }
        else if (strcmp(instr_ptr->tokens[0], "cp") == 0)
        {
        }
        else
        {
            //Print error because that command does not exist
            printf("%s: Instruction is not recognized\n", instr_ptr->tokens[0]);
        }
    }
}
/*
int main()
{
    char command[100];

    scanf("%s", command);

    while (strcmp(command, "exit") != 0)
    {
        printf("$ ");
        scanf("%s", command);
        if (strcmp(command, "info") == 0)
        {
            printf("Print the info for the file\n");
        }
        else if (strcmp(command, "ls") == 0)
        {
            printf("Print directory contents\n");
        }
    }
}*/