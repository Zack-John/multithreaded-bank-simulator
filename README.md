# Duck Bank and Puddles Bank: A Multi-threaded Bank System Simulator


## Description
The ultimate goal of the project was to gain experience in multi-process / multi-threaded programming, system calls, and the tools available to address race conditions (i.e., mutex locks) and prevent deadlocks.
This program provides a thread-safe solution to handle hundreds of thousands of simulated transaction requests, "authenticate" each transaction (using simple plaintext passwords), apply the appropriate changes to each account, calculate checking account rewards, and simulate a separate savings account for each account (with a secondary bank, the "Puddles Bank") inside of a separate process using shared memory space.


## How it Works
The application begins by building the bank accounts using the info provided in the input file (the format for the input file is described in the "How to Use" section below). It then parses the list of transaction requests and distributes the workload among ten "worker" threads for processing. Requests include "transfer funds", "deposit", "withdraw", and "check account balance". For each transaction request, the worker thread identifies the requesting account, verifies the password provided with the request matches the password associated with the account, and then performs whatever account action was requested. In addition to the ten worker threads, there is also a main "bank" thread that is responsible for applying rewards to the accounts. Each account has a custom reward rate, and whenever a deposit, withdrawl, or transfer is initiated by an account, the value of the transaction is added to a value tracker inside that account. Once the number of transaction requests handled across _all_ worker threads have reached a certain threshold (5000 by default), all worker threads will pause their operation and notify the primary "bank" thread that it is time to update. Once the bank thread finishes updating the accounts, it notifies the worker threads to resume processing. This process repeats until all transaction requests are handled.

Additionally, each user also has a savings account at a separate institution, Puddles Bank (represented by a separate process; two processes total). Every account is "duplicated" at Puddles Bank, with an initial balance equal to 20% of their checking account balance and a flat interest rate of 2%. The Duck Bank process writes the account info to shared memory where Puddles Bank reads and updates from. Every time the "bank" thread applies rewards to the Duck Bank accounts, the Puddles Bank processes applies interest to the savings accounts.

## How to Use
This repository contains all source files, a Makefile for building the program, and an example input file. To use the application:
1. Navigate to the directory where you've downloaded the files, then navigate into the part 4 folder
2. Use the 'make' command to build the executable
3. Use the command './bank input.txt' to execute the program
4. Final account balances are printed to a file called 'output.txt' in the same directory

### Input file structure:
● First line:

Line 1: n total # of accounts


● Account block:

Line n: index # indicating the start of account information

Line n + 1: #........# account number (char*)

Line n + 2: ****** password (char*)

Line n + 3: ###### initial balance (double)

Line n + 4: #.## reward rate (double)


● Transaction lines (separated by space)

Transfer funds: “T src_account password dest_account transfer_amount”

Check balance: “C account_num password”

Deposit: “D account_num password amount”

Withdraw: “W account_num password amount


This program was originally developed as the final project for an Operating Systems course at the University of Oregon by Zack Johnson.
