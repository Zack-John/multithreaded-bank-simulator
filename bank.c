#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include "string_parser.h"
#include "account.h"



/* ---------- P A R T  1 ----------*/
// TODO: REMARKS:
// "RACE CONDITIONS WILL PLAY A HUGE ROLE IN PART 3"

// AN IMPORTANT QUESTION YOU SHUOLD ASK YOURSELF IS:
// "HOW DO YOU MAKE SURE ONE THREAD WILL REACH A CERTAIN PART
// OF THE CODE BEFORE ANOTHER?"

// DEADLOCKS COULD MAKE YOUR PROGRAM STUC, AND IT IS
// EXTREMELY DIFFICULT TO FIGURE OUT EXACTLY WHAT HAPPEND
// AND HOW TO RESOLVE IT. THINK ABOUT WHAT VARIABLES YOU
// COULD KEEP TRACK OF TO SIGNAL A DEADLOCK!
/* --------------------------------*/


// TESTING: used to track # of transactions, etc
// (for debugging)
int transfers = 0;
int withdraws = 0;
int checks = 0;
int deposits = 0;
int bad_pass = 0;
int bad_acct = 0;

int num_accounts = 0;
int balance_updates = 0;

account * account_array;


void * process_transaction(command_line * arg);
void * update_balance();


int main(int argc, char* argv[]) {

    // check usage
    if (argc != 2) {
        printf("Usage: ./bank input.txt\n");
        return 1;
    }

    // check output directory
    DIR * dir = opendir("./output");
    if (dir == NULL) {
        perror("Failed to open directory");
        return 1;
    }
    closedir(dir);

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

    // NOTE: an array might be really slow here
    // if its an issue, use a hashmap
    account_array = (account *)malloc(sizeof(account) * num_accounts);

    for (int i = 0; i < num_accounts; i++) {

        // create struct
        account entry;

        // NOTE: i dont think i technically need to remove the newlines below...
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

        //--- transaction tracker starts at 0
        entry.transaction_tracker = 0.00;

        //--- init out_file path
        char out_path[64];
        sprintf(out_path, "./output/account%d.txt", i);
        strcpy(entry.out_file, out_path);

        // write initial balance to file

        // store the new entry in the array
        account_array[i] = entry;
    }

    // init output files
    // try with single / multiple file pointers if issues arise
    for (int i = 0; i < num_accounts; i++) {
        FILE * fp2 = fopen(account_array[i].out_file, "w");
        if (fp2 == NULL) {
            perror("failed to init out_file");
            return 1;
        }

        fprintf(fp2, "account: %d\n", i);
        fclose(fp2);
    }

    // printf("FILE INIT LOOP COMPLETE\n");

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
    for (int i = 0; i < num_transactions; i++) {                  // FIXME: num_transactions / set amt for debugging
        // printf("transaction #%d: ", i);
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
    printf("total: %d\n\n", (transfers + checks + deposits + withdraws + bad_pass));

    // calculate rewards
    int * out = update_balance(account_array);
    printf("Total balance updates: %d\n", *out);

    // write out final balances to output.txt
    FILE * outfp = fopen("output.txt", "w");
    for (int i = 0; i < num_accounts; i++) {
        fprintf(outfp, "%d balance:\t%.2f\n\n", i, account_array[i].balance);
    }

    // cleanup
    fclose(fp);
    fclose(outfp);

    free(line_buf);
    free(account_array);

    for (int i = 0; i < num_transactions; i++) {
        free_command_line(&transactions[i]);
    }

    return 0;
}




void * process_transaction(command_line * arg) {
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
        printf("BAD PASSWORD for account %s!\n", account_array[account_index].account_number);
        bad_pass++;
        return arg;
    }

    // if passwords match, handle transaction:

    // open SRC output file (in append mode!)
    // FILE * src_fp = fopen(account_array[account_index].out_file, "a");
    // if (src_fp == NULL) {
    //     perror("Failed to open SOURCE output file");
    //     return arg;
    // }

    /* TRANSFER */
    if (strcmp(tran.command_list[0], "T") == 0) {

        transfers++;

        // get dest account
        int dest_index = -1;
        for (int i = 0; i < num_accounts; i++) {
            if (strcmp(account_array[i].account_number, tran.command_list[3]) == 0) {
                dest_index = i;
                break;
            }
        }

        // make sure we found the dest account
        if (dest_index == -1) {
            printf("DEST ACCOUNT NOT FOUND: %s\n", tran.command_list[3]);
            return arg;
        }

        // get value of transfer
        double val = atof(tran.command_list[4]);

        // terminal output
        printf("Transfer: %s ----> %s (%.2f)\n", tran.command_list[1], tran.command_list[3], val);

        // sending account: remove val from balance
        account_array[account_index].balance -= val;

        // sending account: add val to rewards tracker
        account_array[account_index].transaction_tracker += val;

        // receiving account: add val to balance
        account_array[dest_index].balance += val;

        // write out new balance for SOURCE
        // fprintf(src_fp, "Current Balance:\t%.2f\n", account_array[account_index].balance);

        // immediately close SRC fp
        // fclose(src_fp);

        // open DEST output file (in append mode!)
        // FILE * dest_fp = fopen(account_array[dest_index].out_file, "a");
        // if (dest_fp == NULL) {
        //     perror("Failed to open DESTINATION output file");
        //     return arg;
        // }

        // write out new balance for DEST
        // fprintf(dest_fp, "Current Balance:\t%.2f\n", account_array[dest_index].balance);

        // immediately close DEST fp
        // fclose(dest_fp);
    }

    /* CHECK BALANCE */
    if (strcmp(tran.command_list[0], "C") == 0) {
        
        checks++;

        printf("Check Balance:\t%.2f\n", account_array[account_index].balance);

        // write out balance to output file
        // fprintf(src_fp, "Current Balance:\t%.2f\n", account_array[account_index].balance);

        // close fp
        // fclose(src_fp);
    }

    /* DEPOSIT */
    if (strcmp(tran.command_list[0], "D") == 0) {

        deposits++;

        // get the deposit amount
        double val = atof(tran.command_list[3]);

        printf("Deposit: %s - %.2f\n", tran.command_list[1], val);

        // add the deposited amount to balance
        account_array[account_index].balance += val;

        // adjust reward tracker value
        account_array[account_index].transaction_tracker += val;

        // write out new balance to output file
        // fprintf(src_fp, "Current Balance:\t%.2f\n", account_array[account_index].balance);

        // close fp
        // fclose(src_fp);
    }

    /* WITHDRAW */
    if (strcmp(tran.command_list[0], "W") == 0) {

        withdraws++;

        // get the withdraw amount
        double val = atof(tran.command_list[3]);

        printf("Withdraw: %s - %.2f\n", account_array[account_index].account_number, val);

        // remove the withdrawn amount from balance
        account_array[account_index].balance -= val;

        // adjust reward tracker value
        account_array[account_index].transaction_tracker += val;


        // write out new balance
        // fprintf(src_fp, "Current Balance:\t%.2f\n", account_array[account_index].balance);

        // close fp
        // fclose(src_fp);
    }

    return arg;
}


void * update_balance() {

    /* this function will return the number
    of times it had to update each account */

    FILE * out_fp;

    for (int i = 0; i < num_accounts; i++) {

        // get reward value (tracker value * reward rate)
        double reward = account_array[i].transaction_tracker * account_array[i].reward_rate;

        // add reward value to account balance
        account_array[i].balance += reward;

        // open out_file (in append mode!)
        out_fp = fopen(account_array[i].out_file, "a");
        if (out_fp == NULL) {
            perror("Failed to open out_file");
            break;  // maybe exit here?
        }

        // write new balance to out_file
        fprintf(out_fp, "Current Balance:\t%.2f\n", account_array[i].balance);

        // close out_file
        fclose(out_fp);
    }
    
    // increment update counter
    balance_updates++;
    // void * return_val = &balance_updates;
    return &balance_updates;
}
