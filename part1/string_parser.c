/*
 * string_parser.c
 *
 *  Created on: Nov 25, 2020
 *      Author: gguan, Monil
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GUN_SOURCE


//this functions returns the number of tokens needed for the string array
//based on the delimeter
int count_token (char* buf, const char* delim)
{
	// check for NULL string
	// if (buf == NULL) { return 0; }

	// init stuff
	char * saveptr;
	char * token;
	int count = 0;

	token = strtok_r(buf, delim, &saveptr);
	while (token != NULL) {
		count++;
		token = strtok_r(NULL, delim, &saveptr);
	}

	return count;
}


//This functions can tokenize a string to token arrays base on a specified delimeter,
//it returns a struct variable
command_line str_filler (char* buf, const char* delim)
{
	command_line output;

	char * saveptr;

	// remove newline from buf
	char * buf_wo_newline = strtok_r(buf, "\n", &saveptr);

	// make copy to give to count fxn
	char * buf_copy = strdup(buf_wo_newline);

	// count tokens
	output.num_token = count_token(buf_copy, delim);

	// free the copy
	free(buf_copy);

	// malloc memory for token array inside command_line variable based on number of tokens
	int num_elements = output.num_token + 1;
	output.command_list = (char **)malloc(sizeof(char *) * num_elements);

	// get first token
	char * token = strtok_r(buf_wo_newline, delim, &saveptr);
	output.command_list[0] = strdup(token);

	// get rest of tokens
	int i;
	for (i = 1; i < output.num_token; i++) {

		// get next token
		token = strtok_r(NULL, delim, &saveptr);

		if (token == NULL) { break; }
		else { output.command_list[i] = strdup(token);}
	}

	// make last element null
	output.command_list[i] = NULL;

	return output;
}


//this function safely free all the tokens within the array.
void free_command_line(command_line* command)
{
	
	for (int i = 0; i <= command->num_token; i++) {
		free(command->command_list[i]);
	}

	free(command->command_list);
}
