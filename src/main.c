#include "sharam.h"

int
main(int argc, char *argv[])
{
	char line[MAXLINE];
	char pwd[PATH_MAX];

	//int i; // Unused variable when not debugging
	ShellState shell;

	initialize_shell(&shell);
	Redirection redir = { NULL, NULL, NULL, 0 };
	Command cmd;

	// The program ends when EOF (ctrol+D)
	while (69) {
		if (getcwd(pwd, sizeof(pwd)) == NULL) {
			strcpy(pwd, "[error: cannot get current directory]");
		}
		printf("\033[1;92m%s\033[0m", pwd);
		printf("\033[1;36m sharam> \033[0m");

		if (fgets(line, MAXLINE, stdin) != NULL) {
			// Passing them by value so to modify the context
			line_parser(line, &shell, &redir);
		} else {
			// EOF detected (end of script file)
			break;	// Exit the loop and terminate the shell
		}
		//For debugging
		// for (i = 0; i < shell.arg_count; i++) {
		//      printf("arg[%d]: %s\n", i, shell.arguments[i]);
		// } 

		// This means no error during parse and hence is theoretically a valid Command
		if (!shell.var_error && shell.arg_count > 0) {
			cmd = new_command(shell.arguments[0]);
			if (shell.arg_count > 1
			    && strcmp(shell.arguments[shell.arg_count - 1],
				      "&") == 0) {
				cmd.background = 1;
				free(shell.arguments[shell.arg_count - 1]);
				shell.arguments[shell.arg_count - 1] = NULL;	// To not pass it as argument
			} else
				cmd.background = 0;

			// Handling here document before forking
			if (shell.heredoc_active == ACTIVEHEREDOC) {
				printf("> ");
				// Read here document content
				while (fgets(line, MAXLINE, stdin) != NULL) {
					if (strcmp(line, "}\n") == 0) {
						break;
					}
					// Store the content in the pipe
					if (write
					    (shell.herePipes[1], line,
					     strlen(line)) != strlen(line)) {
						warnx
						    ("Could not write to pipe");
						break;
					}
					printf("> ");
				}
				// Closing write end after writing
				close(shell.herePipes[1]);
			}

			execute_command(cmd, &shell, &redir);

			// Cleanup resources
			cleanup_command(&cmd);
			cleanup_redirection(&redir);
		}
		cleanup_arguments(&shell);
	}

	// Final cleanup before exiting
	cleanup_shell(&shell);
	exit(EXIT_SUCCESS);
}
