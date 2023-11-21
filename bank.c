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

    // declare line buffers
    size_t len = 0;
    char * line_buf = NULL;

    command_line token_buffer;

    // open input file
    FILE * fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("Unable to open input file");
        return 1;
    }

    // get num of accounts
    getline(&line_buf, &len, fp);
    int num_accounts = atoi(line_buf);

    // an array might be really slow here
    // if its an issue, use a hashmap
    account account_array[num_accounts];

    for (int i = 0; i < num_accounts; i++) {

        // create struct
        account entry;

        // i dont think i technically need to remove the newlines below...
        // i think i could just call it with empty delim arg for strings
        // or skip it all together when doing atoi, atof, etc

        //--- line n: index number
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");
        printf("----- %s -----\n", line_buf);
        free_command_line(&token_buffer);

        //--- line n + 1: account number (char *)
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");
        strcpy(entry.account_number, token_buffer.command_list[0]);
        free_command_line(&token_buffer);

        //---  line n + 2: password (char *)
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");
        strcpy(entry.password, token_buffer.command_list[0]);
        free_command_line(&token_buffer);

        //--- line n + 3: initial balance (double)
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");
        // sscanf(token_buffer.command_list[0], "%lf", &entry.balance);
        entry.balance = atof(token_buffer.command_list[0]);
        free_command_line(&token_buffer);

        //--- line n + 4: reward rate (double)
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, "\n");
        entry.reward_rate = atof(token_buffer.command_list[0]);
        free_command_line(&token_buffer);

        // store the new entry in the array
        account_array[i] = entry;
    }

    // TESTING: print account array
    for (int i = 0; i < num_accounts; i++) {
        printf("account %d: %s\n", i, account_array[i].account_number);
    }

    // get current position in the file
    long int pos = ftell(fp);

    // get num of transactions
    int num_transactions = 0;
    while (getline(&line_buf, &len, fp) != -1) {
        num_transactions++;
    }

    // init transaction array
    command_line transactions[num_transactions];

    // return to top of transactions
    fseek(fp, pos, SEEK_SET);

    /* GET TRANSACTION INFO */
    // transfers:   T src_account password dest_account transfer_amount
    // check bal:   C account_num password
    // deposit:     D account_num password amount
    // withdraw:    W account_num password amount

    // tokenize transactions, store in array
    for (int i = 0; i < num_transactions; i++) {
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, " ");

        transactions[i] = token_buffer;
    }

    // cleanup
    fclose(fp);
    free(line_buf);

    for (int i = 0; i < num_transactions; i++) {
        free_command_line(&transactions[i]);
    }

    return 0;
}
