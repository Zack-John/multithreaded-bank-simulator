#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>

#include <pthread.h>

#include "string_parser.h"
#include "account.h"


void * process_transaction(void * arg);
void * update_balance(void * arg);

int main(int argc, char* argv[]) {

    // check usage
    if (argc != 2) {
        printf("Usage: ./bank input.txt\n");
        return 1;
    }

    // declare line buffer
    size_t len = 0;
    char * line_buf = NULL;

    // open input file
    FILE * fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("Unable to open input file");
        return 1;
    }

    // get num of accounts
    getline(&line_buf, &len, fp);
    int num_accounts = atoi(line_buf);

    command_line token_buffer;

    // an array might be really slow here
    // if its an issue, use a hashmap
    account account_array[num_accounts];

    for (int i = 0; i < num_accounts; i++) {

        // create (and malloc?) struct
        account entry;

        //--- line n: index number
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");  // get rid of newline
        printf("index: %s\n", line_buf);

        //--- line n + 1: account number (char *)
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");
        strcpy(entry.account_number, token_buffer.command_list[0]);
        printf("entry.account_number: %s\n", entry.account_number);

        //---  line n + 2: password (char *)
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");  // get rid of newline
        printf("password: %s\n", line_buf);

        //--- line n + 3: initial balance (double)
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");  // get rid of newline
        // sscanf(token_buffer.command_list[0], "%lf", &entry.balance);
        entry.balance = atof(token_buffer.command_list[0]);
        
        printf("initial balance: %s\n", line_buf);
        printf("entry.balance: %lf\n", entry.balance);

        //--- line n + 4: reward rate (double)
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");  // get rid of newline
        printf("reward rate: %s\n", line_buf);
    }

    /* GET TRANSACTION INFO */
    // 1. read each line of file after the account info lines
    // 2. use command_line struct to tokenize them for different fields

    // TESTING: print the rest of the file (transaction lines)
    // while (getline(&line_buf, &len, fp) != -1) {
    //     printf("%s", line_buf);
    // }
    // printf("\n");


    fclose(fp);
    free(line_buf);

    return 0;
}
