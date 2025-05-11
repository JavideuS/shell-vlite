#include "sharam.h"

char *
int_to_string(int value)
{
	char buffer[32];

	snprintf(buffer, sizeof(buffer), "%d", value);

	//      Alternative to strdup 
	// No strdup alt
	// size_t len = strlen(buffer) + 1;
	// char* str = malloc(len);
	// if (str) {
	//     memcpy(str, buffer, len);
	// }
	//return str;
	return strdup(buffer);
}

void
initialize_shell(ShellState *shell)
{
	shell->local_var_capacity = 5;
	shell->arg_capacity = 10;	// Initial argument capacity will be 10, in case it isn't enough, it will realloc the double
	shell->arg_count = 0;
	shell->local_var_count = 0;
	shell->var_error = 0;
	shell->heredoc_active = ACTIVEHEREDOC - 1;	// To not match, meaning no heredoc

	// Allocating memory for local variables
	shell->local_var =
	    (LocalVariables *) calloc(shell->local_var_capacity,
				      sizeof(LocalVariables));
	if (!shell->local_var) {
		err(1, "Memory allocation failed for local variables");
	}
	// Initializing default variable $result for exit status
	shell->local_var[shell->local_var_count].name = strdup("result");
	if (!shell->local_var[shell->local_var_count].name) {
		free(shell->local_var);
		err(1, "Memory allocation failed for variable name");
	}

	shell->local_var[shell->local_var_count].value = strdup("0");
	if (!shell->local_var[shell->local_var_count].value) {
		free(shell->local_var[shell->local_var_count].name);
		free(shell->local_var);
		err(1, "Memory allocation failed for variable value");
	}

	shell->local_var_count++;

	// Allocating memory for arguments
	shell->arguments = calloc(shell->arg_capacity, sizeof(char *));
	if (!shell->arguments) {
		cleanup_shell(shell);
		err(1, "Memory allocation failed for arguments");
	}
}

void
cleanup_shell(ShellState *shell)
{
	int i;

	// Freeing all local variables
	for (i = 0; i < shell->local_var_count; i++) {
		if (shell->local_var[i].name) {
			free(shell->local_var[i].name);
			shell->local_var[i].name = NULL;
		}
		if (shell->local_var[i].value) {
			free(shell->local_var[i].value);
			shell->local_var[i].value = NULL;
		}
	}

	if (shell->local_var) {
		free(shell->local_var);
		shell->local_var = NULL;
	}
	// Free arguments array
	if (shell->arguments) {
		cleanup_arguments(shell);
		free(shell->arguments);
		shell->arguments = NULL;
	}
}

void
cleanup_redirection(Redirection *redir)
{
	if (redir->inputFile != NULL) {
		free(redir->inputFile);
		redir->inputFile = NULL;
	}
	if (redir->outputFile != NULL) {
		free(redir->outputFile);
		redir->outputFile = NULL;
	}
	if (redir->errorFile != NULL) {
		free(redir->errorFile);
		redir->errorFile = NULL;
	}
}

void
cleanup_arguments(ShellState *shell)
{
	int i;

	for (i = 0; i < shell->arg_count; i++) {
		if (shell->arguments[i]) {
			free(shell->arguments[i]);
			shell->arguments[i] = NULL;
		}
	}
	shell->arg_count = 0;
	shell->var_error = 0;
}
