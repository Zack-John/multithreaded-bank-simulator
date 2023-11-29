#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include "string_parser.h"
#include "account.h"



/* ---------- P A R T  3 ----------*/

// RUBRIC:
// 10 points:
// correct answer is reached and account#.txt is different every run,
// however account#.txt should have the same number of lines every run

// 15 points:
// correct usage of pthread_barrier_wait

// 10 points:
// program does not deadlock

// 15 points:
// correct usage of pthread_cond_wait and pthread_cond_broadcast / signal

typedef struct workload_manager {
    int thread_idx;
    int workload;
} workload_manager;

int workload = 0;
int num_accounts = 0;
int balance_updates = 0;

int threads_done = 0;
int transaction_counter = 0;

pthread_mutex_t bank_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bank_cond = PTHREAD_COND_INITIALIZER;

pthread_barrier_t start_barrier; // c17 -> gnu99

pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t update_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t update_done_cond = PTHREAD_COND_INITIALIZER;

account * account_array;
command_line * transactions;

// TESTING
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;


void * process_transaction(void * arg);
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

        //--- init transaction tracker 
        entry.transaction_tracker = 0.00;

        //--- init out_file
        char out_path[64];
        sprintf(out_path, "./output/account%d.txt", i);
        strcpy(entry.out_file, out_path);

        //--- init account mutex
        pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
        entry.ac_lock = mutex;

        // store the new entry in the array
        account_array[i] = entry;
    }

    // init output files
    for (int i = 0; i < num_accounts; i++) {
        FILE * fp2 = fopen(account_array[i].out_file, "w");
        if (fp2 == NULL) {
            perror("failed to init out_file");
            return 1;
        }

        fprintf(fp2, "account: %d\n", i);
        fclose(fp2);
    }

    // get current position in the file
    long int pos = ftell(fp);

    // get num of transactions
    int num_transactions = 0;
    while (getline(&line_buf, &len, fp) != -1) {
        num_transactions++;
    }

    // init transaction array
    // command_line transactions[num_transactions];
    transactions = (command_line *)malloc(sizeof(command_line) * num_transactions);

    // return to top of transactions
    fseek(fp, pos, SEEK_SET);

    // tokenize transactions, store in array
    for (int i = 0; i < num_transactions; i++) {
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, " ");
        transactions[i] = token_buffer;
    }

    // get number of transactions for each thread
    // ("evenly slice the number of transactions")
    workload = (num_transactions / 10);

    // init barriers
    pthread_barrier_init(&start_barrier, NULL, 11); // 10 workers + 1 main thread

    // init bank thread
    pthread_t bank_thread;

    // init thread array
    pthread_t * threads;
    threads = (pthread_t *)malloc(sizeof(pthread_t) * 10);

    // create bank thread
    if (pthread_create(&bank_thread, NULL, &update_balance, NULL) != 0 ) {
            perror("FAILED TO CREATE BANK THREAD");
            return 1;
        }

    // create worker threads
    for (int i = 0; i < 10; i++) {
        // index calculation w 12k workload:
        // 0 * 12k = 0 -----> 11999
        // 1 * 12k = 12k ---> 23999
        // 2 * 12k = 24k ---> 35999
        // ...
        // 9 * 12k = 108k ---> 119999

        // init thread manager struct
        workload_manager * mgr = malloc(sizeof(workload_manager));
        mgr->workload = workload;
        mgr->thread_idx = i;

        if (pthread_create(threads + i, NULL, &process_transaction, mgr) != 0 ) {
            perror("FAILED TO CREATE WORKER THREAD");
            return 1;
        }
    }

    // wait here until all threads are created and ready to run
    pthread_barrier_wait(&start_barrier);

    // wait for worker threads to finish
    for (int i = 0; i < 10; i++) {
        int ex = pthread_join(threads[i], NULL);
        printf("[main] thread %d finished with code %d!\n", i, ex);
    }

    printf("[main] all workers done!\n");

    // all our worker threads are finished,
    // send bank thread a signal to wake it
    pthread_cond_signal(&update_cond);

    // wait for bank thread to finish    
    int * out;
    pthread_join(bank_thread, (void **)&out);

    // print total number of updates
    printf("Total balance updates: %d\n", *out);

    // write out final balances to output.txt
    FILE * outfp = fopen("output.txt", "w");
    for (int i = 0; i < num_accounts; i++) {
        // printf("%d balance:\t%.2f\n\n", i, account_array[i].balance);           // TODO: remove this line when done
        fprintf(outfp, "%d balance:\t%.2f\n\n", i, account_array[i].balance);
    }

    // cleanup
    fclose(fp);
    fclose(outfp);

    free(line_buf);
    free(account_array);
    free(threads);

    for (int i = 0; i < num_transactions; i++) {
        free_command_line(&transactions[i]);
    }

    free(transactions);

    return 0;
}


void * process_transaction(void * arg) {

    // wait until all threads are created to start processing
    pthread_barrier_wait(&start_barrier);

    // deref workload data struct
    workload_manager mgr = *(workload_manager *)arg;

    // get starting index for this thread
    int starting_index = (mgr.thread_idx * mgr.workload);

    for (int i = 0; i < mgr.workload; i++) {

        int needs_update = 0;

        // get current transaction to process
        command_line tran = transactions[starting_index + i];

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
            continue;
        }

        // now that we have the account, check the password
        if (strcmp(account_array[account_index].password, tran.command_list[2]) != 0) {
            // printf("BAD PASSWORD for account %s!\n", account_array[account_index].account_number);
            continue;
        }

        // if just checking balance, do nothing
        if (strcmp(tran.command_list[0], "C") == 0) {
            // NOTE: Dewi said we can just do nothing here...
            continue;
        }

        // lock counter mutex
        pthread_mutex_lock(&counter_mutex);

        // increment tracker
        transaction_counter++;

        // if this is #5000, hold on to tracker mutex so other
        // threads have to wait for it to continue processing
        if (transaction_counter == 5000) {

            // we need to update after processing
            needs_update = 1;
        }

        // if this isn't #5000, unlock tracker mutex and continue
        else {
            pthread_mutex_unlock(&counter_mutex);
        }

        /* TRANSFER */
        if (strcmp(tran.command_list[0], "T") == 0) {

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
                continue;
            }

            // get value of transfer
            double val = atof(tran.command_list[4]);

            // printf("Transfer: %s ----> %s (%.2f)\n", tran.command_list[1], tran.command_list[3], val);

            // lock source account mutex
            pthread_mutex_lock(&account_array[account_index].ac_lock);

            // sending account: remove val from balance
            account_array[account_index].balance -= val;

            // sending account: add val to rewards tracker
            account_array[account_index].transaction_tracker += val;

            // unlock src mutex
            pthread_mutex_unlock(&account_array[account_index].ac_lock);

            // lock dest account mutex
            pthread_mutex_lock(&account_array[dest_index].ac_lock);

            // receiving account: add val to balance
            account_array[dest_index].balance += val;

            // unlock dest mutex
            pthread_mutex_unlock(&account_array[dest_index].ac_lock);
        }

        // /* CHECK BALANCE */
        // if (strcmp(tran.command_list[0], "C") == 0) {
        //     // NOTE: Dewi said we can just do nothing here...
        //     continue;
        // }

        /* DEPOSIT */
        if (strcmp(tran.command_list[0], "D") == 0) {

            // get the deposit amount
            double val = atof(tran.command_list[3]);

            // printf("Deposit: %s - %.2f\n", tran.command_list[1], val);

            pthread_mutex_lock(&account_array[account_index].ac_lock);

            // add the deposited amount to balance
            account_array[account_index].balance += val;

            // adjust reward tracker value
            account_array[account_index].transaction_tracker += val;

            pthread_mutex_unlock(&account_array[account_index].ac_lock);
        }

        /* WITHDRAW */
        if (strcmp(tran.command_list[0], "W") == 0) {

            // get the withdraw amount
            double val = atof(tran.command_list[3]);

            pthread_mutex_lock(&account_array[account_index].ac_lock);

            // printf("Withdraw: %s - %.2f\n", account_array[account_index].account_number, val);

            // remove the withdrawn amount from balance
            account_array[account_index].balance -= val;

            // adjust reward tracker value
            account_array[account_index].transaction_tracker += val;

            pthread_mutex_unlock(&account_array[account_index].ac_lock);
        }

        // if this was #5000 we need to update
        if (needs_update) {

            // lock update mutex
            pthread_mutex_lock(&update_mutex);
            
            // signal bank thread for update
            pthread_cond_signal(&update_cond);

            // wait for bank thread to finish update
            // (timedwait for 1 second here to fix an occasional deadlock)
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;

            pthread_cond_timedwait(&update_done_cond, &update_mutex, &ts);

            // reset tracker to 0
            transaction_counter = 0;

            // unlock update mutex once bank is done updating
            pthread_mutex_unlock(&update_mutex);

            // finally, release the tracker mutex to let
            // everyone else through
            pthread_mutex_unlock(&counter_mutex);
        }
    }

    // ONCE THIS THREADS WORKLOAD HAS BEEN PROCESSED...

    // lock the thread mutex
    pthread_mutex_lock(&thread_mutex);

    // increment threads_done
    threads_done++;

    // unlock the thread mutex
    pthread_mutex_unlock(&thread_mutex);

    // signal that we're done (for bank thread to check how many are now done)
    // pthread_cond_signal(&bank_cond);

    // TESTING
    if (threads_done == 10) {
        // printf("[thread exit] THIS IS THE LAST THREAD TO EXIT, SENDING ANOTHER UPDATE SIGNAL\n");
        pthread_cond_signal(&update_cond);
    }
    
    free(arg);

    pthread_exit(0);
}


void * update_balance() {

    /* this function will return the number
    of times it had to update each account */

    // must lock mutex before waiting
    pthread_mutex_lock(&bank_mutex);

    while (threads_done < 10) {

        // wait for signal that we need to update balances
        pthread_cond_wait(&update_cond, &bank_mutex);

        // ^^^ pthread_cond_wait is the same as:
        // pthread_mutex_unlock(bank_mutex)
        // wait for signal on bank_cond
        // pthread_mutex_lock(bank_mutex)

        if (threads_done < 10) {

            // update balances
            FILE * out_fp;

            for (int i = 0; i < num_accounts; i++) {

                // lock account mutex
                pthread_mutex_lock(&account_array[i].ac_lock);

                // get reward value (tracker value * reward rate)
                double reward = account_array[i].transaction_tracker * account_array[i].reward_rate;

                // add reward value to account balance
                account_array[i].balance += reward;

                // open out_file (in append mode!)
                out_fp = fopen(account_array[i].out_file, "a");
                if (out_fp == NULL) {
                    perror("Failed to open out_file");
                    break;  // exit here instead?
                }

                // write new balance to out_file
                fprintf(out_fp, "Current Balance:\t%.2f\n", account_array[i].balance);

                // close out_file
                fclose(out_fp);

                // RESET TRACKER VALUE
                account_array[i].transaction_tracker = 0;

                // release account mutex
                pthread_mutex_unlock(&account_array[i].ac_lock);
            }

            balance_updates++;

            pthread_cond_broadcast(&update_done_cond);
        }

        else if (threads_done == 10) {
            // printf("[bank] ALL THREADS DONE, SHOULD BE EXITING LOOP\n");
        }

    }

    // unlock the bank mutex since we're done
    pthread_mutex_unlock(&bank_mutex);
    
    // return update counter
    return &balance_updates;
}
