#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/mman.h>
#include <signal.h>

#include "string_parser.h"
#include "account.h"



/* ---------- P A R T  4 ----------*/

typedef struct workload_manager {
    int thread_idx;
    int workload;
} workload_manager;


int workload = 0;
int num_accounts = 0;
int balance_updates = 0;

int threads_done = 0;
int transaction_counter = 0;

pthread_barrier_t start_barrier; // c17 -> gnu99

pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t update_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t update_done_cond = PTHREAD_COND_INITIALIZER;

account * account_array;
command_line * transactions;

pid_t pid;
sigset_t sigset;


void * process_transaction(void * arg);
void * update_balance();


int main(int argc, char* argv[]) {

    // check usage
    if (argc != 2) {
        printf("Usage: ./bank input.txt\n");
        return 1;
    }

    // check output directory
    DIR * dir;
    dir = opendir("./output");
    if (dir == NULL) {
        perror("Failed to find output directory");
        return 1;
    }
    closedir(dir);

    dir = opendir("./savings");
    if (dir == NULL) {
        perror("Failed to find savings directory");
        return 1;
    }
    closedir(dir);

    // create an empty sigset_t
    sigemptyset(&sigset);

    // add SIGUSR1 signal to wait set
    if (sigaddset(&sigset, SIGUSR1) != 0) {
        perror("Failed to add signal to set\n");
        return 1;
    }

    // add SIGUSR2 signal to wait set
    if (sigaddset(&sigset, SIGUSR2) != 0) {
        perror("Failed to add signal to set\n");
        return 1;
    }

    // use sigprocmask() to add the signal set in the sigset for blocking
    if (sigprocmask(SIG_SETMASK, &sigset, NULL) != 0) {
        perror("Failed to set proc mask");
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

    // init account array
    account_array = (account *)malloc(sizeof(account) * num_accounts);

    for (int i = 0; i < num_accounts; i++) {

        // create struct
        account entry;

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

    // create shared memory
    account * shared_memory = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    // copy accounts to shared memory
    // and init bal, reward
    for (int i = 0; i < num_accounts; i++) {
        shared_memory[i] = account_array[i];
        shared_memory[i].balance = (shared_memory[i].balance * 0.2);
        shared_memory[i].reward_rate = 0.02;
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
    transactions = (command_line *)malloc(sizeof(command_line) * num_transactions);

    // return to top of transactions
    fseek(fp, pos, SEEK_SET);

    // tokenize transactions, store in array
    for (int i = 0; i < num_transactions; i++) {
        getline(&line_buf, &len, fp);
        token_buffer = str_filler(line_buf, " ");
        transactions[i] = token_buffer;
    }


    /* ---- SAVINGS BANK ---- */
    pid = fork();

    if (pid == 0) {

        // init savings acct array
        char savings_accts[num_accounts][64];

        // init savings output files
        for (int i = 0; i < num_accounts; i++) {

            // generate file path
            char file_path[64];
            sprintf(file_path, "./savings/account%d.txt", i);

            // store file path in arr
            strcpy(savings_accts[i], file_path);

            FILE * save_fp = fopen(savings_accts[i], "w");
            if (save_fp == NULL) {
                perror("failed to init savings file");
                return 1;
            }

            fprintf(save_fp, "account: %d\n", i);
            fprintf(save_fp, "Current Savings Balance  %.2f\n", shared_memory[i].balance);
            fclose(save_fp);
        }

        int sig;

        // do the thing
        while (1) {

            // wait for signals
            sigwait(&sigset, &sig);

            FILE * save_fp;

            // update signal
            if (sig == 10) {

                printf("[puddles] updating savings accounts!\n");

                for (int i = 0; i < num_accounts; i++) {

                    // get savings reward value
                    double val = (shared_memory[i].balance * shared_memory[i].reward_rate);

                    // add to savings balance
                    shared_memory[i].balance += val;

                    // open out_file (in append mode!)
                    save_fp = fopen(savings_accts[i], "a");
                    if (save_fp == NULL) {
                        perror("Failed to open savings file");
                        break;
                    }

                    // write new balance to out_file
                    fprintf(save_fp, "Current Savings Balance  %.2f\n", shared_memory[i].balance);

                    // close out_file
                    fclose(save_fp);
                }

                // let main bank know we're done
                kill(getppid(), SIGUSR1);
            }

            // exit signal
            if (sig == 12) break;
        }

        // cleanup
        fclose(fp);
        free(line_buf);
        free(account_array);
        
        for (int i = 0; i < num_transactions; i++) {
            free_command_line(&transactions[i]);
        }
        free(transactions);

        return 0;
    }
    /* -------------------- */

    // get number of transactions for each thread
    workload = (num_transactions / 10);

    // init barrier
    pthread_barrier_init(&start_barrier, NULL, 11); // 10 workers + 1 bank thread

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

        // init thread manager struct
        workload_manager * mgr = malloc(sizeof(workload_manager));
        mgr->workload = workload;
        mgr->thread_idx = i;

        if (pthread_create(threads + i, NULL, &process_transaction, mgr) != 0 ) {
            perror("FAILED TO CREATE WORKER THREAD");
            return 1;
        }
    }

    // wait for worker threads to finish
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
        printf("[main] thread %d finished!\n", i);
    }
    
    printf("[main] all workers done!\n");

    // wait for bank thread to finish    
    int * out;
    pthread_join(bank_thread, (void **)&out);
    printf("[main] bank thread done!\n");

    // send signal to exit child process
    kill(pid, SIGUSR2);

    // wait for child process to exit
    int wait_res = waitpid(pid, NULL, 0);
    if (wait_res == -1) perror("wait issue");
    else printf("[main] puddles done!\n");

    // print total number of updates
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
    free(threads);

    for (int i = 0; i < num_transactions; i++) {
        free_command_line(&transactions[i]);
    }

    free(transactions);

    munmap(shared_memory, sizeof(*shared_memory));

    return 0;
}


void * process_transaction(void * arg) {

    // deref workload data struct
    workload_manager mgr = *(workload_manager *)arg;

    // wait until all threads are created to start processing
    pthread_barrier_wait(&start_barrier);

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
        if (transaction_counter == 5000) needs_update = 1;
        else pthread_mutex_unlock(&counter_mutex);

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

        /* DEPOSIT */
        if (strcmp(tran.command_list[0], "D") == 0) {

            // get the deposit amount
            double val = atof(tran.command_list[3]);

            // lock mutex
            pthread_mutex_lock(&account_array[account_index].ac_lock);

            // add the deposited amount to balance
            account_array[account_index].balance += val;

            // adjust reward tracker value
            account_array[account_index].transaction_tracker += val;

            // unlock mutex
            pthread_mutex_unlock(&account_array[account_index].ac_lock);
        }

        /* WITHDRAW */
        if (strcmp(tran.command_list[0], "W") == 0) {

            // get the withdraw amount
            double val = atof(tran.command_list[3]);

            // lock mutex
            pthread_mutex_lock(&account_array[account_index].ac_lock);

            // remove the withdrawn amount from balance
            account_array[account_index].balance -= val;

            // adjust reward tracker value
            account_array[account_index].transaction_tracker += val;

            // unlock mutex
            pthread_mutex_unlock(&account_array[account_index].ac_lock);
        }

        // if we need to update...
        if (needs_update) {

            // lock update mutex
            pthread_mutex_lock(&update_mutex);
            
            // signal bank thread for update
            pthread_cond_signal(&update_cond);

            // wait for updates to be done
            pthread_cond_wait(&update_done_cond, &update_mutex);

            // reset tracker to 0
            transaction_counter = 0;

            // unlock update mutex
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

    // if we're the last thread, signal bank to wrap up
    if (threads_done == 10) {
        pthread_mutex_lock(&update_mutex);
        pthread_cond_signal(&update_cond);
        pthread_mutex_unlock(&update_mutex);
    }

    // unlock the thread mutex
    pthread_mutex_unlock(&thread_mutex);
    
    free(arg);

    return NULL;
}


void * update_balance() {

    /* this function will return the number
    of times it had to update each account */

    // make sure bank thread has this lock first
    pthread_mutex_lock(&update_mutex);

    // wait until all threads are ready
    pthread_barrier_wait(&start_barrier);

    while (threads_done < 10) {

        // wait for signal that we need to update balances
        pthread_cond_wait(&update_cond, &update_mutex);

        if (threads_done < 10) {

            // increment balance counter
            balance_updates++;

            // console output
            printf("[bank] updating balances! (%d)\n", balance_updates);

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

            // let the worker threads know they can continue
            pthread_cond_broadcast(&update_done_cond);

            // let the savings bank know that its time to update
            kill(pid, SIGUSR1);

            // wait for savings accounts to be updated
            int signal;
            sigwait(&sigset, &signal);
        }
    }

    // unlock the bank mutex since we're done
    pthread_mutex_unlock(&update_mutex);
    
    // return update counter
    return &balance_updates;
}
