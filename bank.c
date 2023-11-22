#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <errno.h>

#include "string_parser.h"
#include "account.h"



// TESTING: used to track # of transactions, etc
// (for debugging)
int transfers = 0;
int withdraws = 0;
int checks = 0;
int deposits = 0;
int bad_pass = 0;
int bad_acct = 0;

int num_accounts = 0;
account * account_array;




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
    num_accounts = atoi(line_buf);

    printf("num_accounts: %d\n", num_accounts);

    // an array might be really slow here
    // if its an issue, use a hashmap
    // FIXME:
    // account account_array[num_accounts];
    account_array = (account *)malloc(sizeof(account) * num_accounts);


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

        //--- transaction tracker starts at 0 I assume?
        entry.transaction_tracker = 0.00;

        // TODO: will need to add out_file and ac_lock (mutex)

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
    // tokenize transactions, store in array
    for (int i = 0; i < num_transactions; i++) {
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, " ");
        transactions[i] = token_buffer;
    }

    // process transactions one at a time (single threaded environment)
    for (int i = 0; i < 20; i++) {            // FIXME: num_transactions
        // printf("transaction #%d: \n", i);
        process_transaction(transactions + i);
    }


    // TESTING: print # of transactions, etc
    printf("\nSTATS:\n");
    printf("transfers: %d\n", transfers);
    printf("checks: %d\n", checks);
    printf("deposits: %d\n", deposits);
    printf("withdraws: %d\n", withdraws);
    printf("bad_pass: %d\n", bad_pass);
    printf("bad_acct: %d\n", bad_acct);
    printf("total: %d\n", (transfers+checks+deposits+withdraws+bad_pass));


    fclose(fp);
    free(line_buf);
    free(account_array);

    for (int i = 0; i < num_transactions; i++) {
        free_command_line(&transactions[i]);
    }


    return 0;
}




void * process_transaction(void * arg) {
    /* TRANSACTION FORMATS */
    //------------- 0       1       2           3               4
    // transfers:   T src_account password dest_account transfer_amount
    // check bal:   C account_num password
    // deposit:     D account_num password amount
    // withdraw:    W account_num password amount

    // deref arg (transactions + i)
    command_line tran = *(command_line *)arg;

    // find requested account in account_array, store index
    int account_index = -1;
    for (int i = 0; i < num_accounts; i++) {

        if (strcmp(account_array[i].account_number, tran.command_list[1]) == 0) {

            account_index = i;
            break;
        }
    }

    // make sure we found the account
    if (account_index == -1) {
        printf("ACCOUNT %s NOT FOUND!!\n", tran.command_list[1]);
        bad_acct++;
        return arg;
    }

    // now that we have the account, check the password
    if (strcmp(account_array[account_index].password, tran.command_list[2]) != 0) {
        // printf("BAD PASSWORD for account %s!\n", found_account.account_number);
        bad_pass++;
        return arg;
    }

    // if passwords match, handle transaction!
    // else if (strcmp(account_array[account_index].password, tran.command_list[2]) == 0) {
    //     printf("Password accepted!\n");
    // }

    // transfer
    if (strcmp(tran.command_list[0], "T") == 0) {
        // printf("Transfer - %s ----> %s (%f)\n", tran.command_list[1], tran.command_list[3], tran.command_list[4]);
        transfers++;
        return arg;
    }

    // check balance
    if (strcmp(tran.command_list[0], "C") == 0) {
        // printf("Check - %s\n", tran.command_list[1]);
        checks++;
        return arg;
    }

    // deposit
    if (strcmp(tran.command_list[0], "D") == 0) {
        // printf("Deposit - %s\n", tran.command_list[1]);
        deposits++;
        return arg;
    }

    // withdraw
    if (strcmp(tran.command_list[0], "W") == 0) {

        withdraws++;

        // get the withdraw amount
        double val = atof(tran.command_list[3]);
        
        // printf("\n%s - Withdraw: %.2f\n", tran.command_list[1], val);
        // printf("Starting balance: %.2f\n", account_array[account_index].balance);
        // printf("Starting tracker val: %.2f\n", account_array[account_index].transaction_tracker);

        // remove the withdrawn amount from balance
        account_array[account_index].balance -= val;

        // adjust reward tracker value
        account_array[account_index].transaction_tracker += val;

        // printf("New balance: %.2f\n", account_array[account_index].balance);
        // printf("New tracker val: %.2f\n", account_array[account_index].transaction_tracker);
    }

    return arg;
}


void * update_balance(void * arg) {
    // "this function will return the number
    // of times it had to update each account"
    // ????

    return arg;
}
