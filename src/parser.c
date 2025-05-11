#include "sharam.h"

void
line_parser(char *line, ShellState *shell, Redirection *redir)
{
	char *token;
	char *saveptr;
	int i, n;

	char *redirect_pos;

	// Remove trailing newline
	size_t len = strlen(line);

	if (len > 0 && line[len - 1] == '\n') {
		line[len - 1] = '\0';
	}
	// First we separate by lines
	for (i = 0, token = strtok_r(line, " \t", &saveptr); token != NULL;
	     token = strtok_r(NULL, " \t", &saveptr), i++) {
		if (shell->arg_count >= shell->arg_capacity) {
			int new_capacity = shell->arg_capacity * 2;	// Duplicate capacity
			char **new_args = realloc(shell->arguments,
						  new_capacity *
						  sizeof(char *));
			if (!new_args) {
				warnx("Memory allocation failed");
				shell->var_error = 1;
				break;
			}
			shell->arguments = new_args;
			shell->arg_capacity = new_capacity;

			// The extra memory from realloc is garbage
			// So we need to manually set it to Null
			for (n = shell->arg_count; n < shell->arg_capacity; n++) {
				shell->arguments[n] = NULL;
			}
		}
		//printf("token: %s\n", token); //For debugging
		if (token[0] == '$') {
			if (handle_variable_substitution(token, shell)) {
				// Go for next token
				continue;
			} else {	// This means nothing was found
				break;
			}
		}
		// Handling Redirection operators
		else {
			redirect_pos = NULL;

			if ((redirect_pos = strpbrk(token, "><")) != NULL) {
				if (!redirection_handler
				    (token, &saveptr, redirect_pos, shell,
				     redir)) {
					// This means error in Redirection
					break;
				}
			} else {
				// Else we simply save the token as argument
				shell->arguments[shell->arg_count++] =
				    strdup(token);
			}
		}

		if (i == 0) {
			if (variable_assigner(token, shell) > 0) {
				//It means that a variable was assigned so we remove it as argument
				shell->arg_count--;
				free(shell->arguments[shell->arg_count]);
				shell->arguments[shell->arg_count] = NULL;
			}
		}
	}
	// Check for heredoc
	check_here_doc(shell, *redir);
}

int
handle_variable_substitution(char *token, ShellState *shell)
{
	char *var_name = token + 1;	// Skipping $ character
	int n;

	if (var_name == NULL || *var_name == '\0') {
		printf("error: empty variable name\n");
		shell->var_error = 1;
		return 0;
	}

	for (n = 0; n < shell->local_var_count; n++) {
		if (shell->local_var[n].name &&
		    strcmp(shell->local_var[n].name, var_name) == 0) {
			// Found the variable, add its value as an argument
			char *value = strdup(shell->local_var[n].value);

			if (!value) {
				warnx("Memory allocation failed");
				shell->var_error = 1;
				return 0;
			}
			shell->arguments[shell->arg_count++] = value;
			return 1;
		}
	}

	// Variable not found
	printf("error: var %s does not exist\n", var_name);
	shell->var_error = 1;
	return 0;
}

int
parse_redirection(char *redirect_pos, char **saveptr, ShellState *shell,
		  Redirection *redir, int mode)
{
	/*
	   Mode 0 -> Input Redirection
	   Mode 1 -> Output Redirection
	 */
	char *token;
	char *redirToken;
	char saved;

	if (redirect_pos[1] == '\0') {
		// Format: "> " - need next token as filename
		token = strtok_r(NULL, " \t", saveptr);
		if (token != NULL) {
			//&& !mode
			//&& mode -> alt syntax
			if (redir->inputFile != NULL && mode == 0) {
				free(redir->inputFile);
				redir->inputFile = NULL;
			} else if (redir->outputFile != NULL && mode == 1) {
				free(redir->outputFile);
				redir->outputFile = NULL;
			}

			if ((redirToken =
			     strpbrk(redirect_pos + 1, "><")) != NULL) {
				saved = *redirToken;
				*redirToken = '\0';
				if (mode) {
					redir->outputFile =
					    strdup(redirect_pos + 1);
				} else {
					redir->inputFile =
					    strdup(redirect_pos + 1);
				}
				*redirToken = saved;

				*saveptr = redirToken;
			} else {
				if (mode) {
					redir->outputFile = strdup(token);
				} else {
					redir->inputFile = strdup(token);
				}
			}
		} else {
			printf("error: missing file for Redirection\n");
			shell->var_error = 1;
			// break;
			return 0;
		}
	} else {
		// Format: ">file" - filename is in this token
		// Verifying there's not multiple single chaining Redirections in one string
		// In that case it would priotize last Redirection (like in bash)
		// Although it wont create files for previous Redirections (they are simply ignored)
		if (redir->inputFile != NULL && mode == 0) {
			free(redir->inputFile);
			redir->inputFile = NULL;
		} else if (redir->outputFile != NULL && mode == 1) {
			free(redir->outputFile);
			redir->outputFile = NULL;
		}

		if ((redirToken = strpbrk(redirect_pos + 1, "><")) != NULL) {
			saved = *redirToken;
			*redirToken = '\0';
			if (mode) {
				redir->outputFile = strdup(redirect_pos + 1);
			} else {
				redir->inputFile = strdup(redirect_pos + 1);
			}
			*redirToken = saved;

			*saveptr = redirToken;
		} else {
			if (mode) {
				redir->outputFile = strdup(redirect_pos + 1);
			} else {
				redir->inputFile = strdup(redirect_pos + 1);
			}
		}
	}
	return 1;
}

// In order to modify context token and saveptr need to be passed as double pointer (by reference)
int
redirection_handler(char *token, char **saveptr, char *redirect_pos,
		    ShellState *shell, Redirection *redir)
{
	int index = redirect_pos - token;
	char saved;

	if (index > 0) {
		// There's text before the <
		saved = *redirect_pos;
		*redirect_pos = '\0';
		shell->arguments[shell->arg_count++] = strdup(token);
		*redirect_pos = saved;
	}

	if (redirect_pos[0] == '<') {
		if (!parse_redirection(redirect_pos, saveptr, shell, redir, 0)) {
			// This means error in Redirection
			return 0;
		}
	} else {
		if (redirect_pos[1] == '>') {
			redir->appendMode = 1;
			if (!parse_redirection
			    (redirect_pos + 1, saveptr, shell, redir, 1)) {
				// This means error in Redirection
				return 0;
			}
		} else {
			redir->appendMode = 0;
			if (!parse_redirection
			    (redirect_pos, saveptr, shell, redir, 1)) {
				// This means error in Redirection
				return 0;
			}
		}
	}
	// Succesfully redirected
	return 1;
}

void
check_here_doc(ShellState *shell, Redirection redir)
{
	//First we verify it doesn't contain any Redirection
	if (redir.inputFile != NULL || redir.outputFile != NULL) {
		//printf("error: here doc cannot be used with Redirection\n"); // For debugging
		return;
	}

	if (shell->arg_count > 0
	    && strcmp(shell->arguments[shell->arg_count - 1], "HERE{") == 0) {
		//It means there was here doc
		free(shell->arguments[shell->arg_count - 1]);
		shell->arg_count--;
		shell->arguments[shell->arg_count] = NULL;

		if (pipe(shell->herePipes) == -1) {
			warnx("Pipe creation failed");
			shell->var_error = 1;
			return;
		}

		shell->heredoc_active = ACTIVEHEREDOC;
	}
	return;
}
