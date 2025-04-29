#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>		//For dup2
#include <unistd.h>		//For getcwd
#include <linux/limits.h>	//For PATH_MAX
#include <errno.h>		//For errno
#include <err.h>		//For warnings/errors
#include <sys/wait.h>		//For wait

#include <signal.h>		//For waiting background processes

// To open folders
#include <sys/types.h>
#include <dirent.h>

enum {
	MAXLINE = 2048,		// Retrieved from getconf LINE_MAX in my terminal
};

typedef struct command {
	int type;
	char *path;
	int builtinIndex;
	int background;
} command;

typedef struct localVariables {
	char *name;
	char *value;
} localVariables;

typedef struct redirection {
	char *inputFile;
	char *outputFile;
	char *errorFile;
	int appendMode;
} redirection;

typedef struct shellState {
	char **arguments;
	int arg_capacity;
	int arg_count;
	localVariables *local_var;
	int local_var_capacity;
	int local_var_count;
	int var_error;
} shellState;

command lookCommand(char *);
command lookBuiltin(char *);
command lookPath(const char *);
command lookCurDir(const char *);
void lineParser(char *, shellState *, redirection *);

// int handleVariableSubstitution(char* , shellState *);

int
main(int argc, char *argv[])
{
	char line[MAXLINE];
	char pwd[PATH_MAX];
	shellState shell;

	shell.local_var_capacity = 5;
	shell.arg_capacity = 10;	// Intial argument capacity will be 10, in case it isn't enough, it will realloc the double
	shell.arg_count = 0;
	shell.local_var_count = 0;
	shell.local_var =
	    (localVariables *) malloc(shell.local_var_capacity *
				      sizeof(localVariables));
	shell.arguments = calloc(shell.arg_capacity, sizeof(char *));
	if (!shell.arguments) {
		free(shell.local_var);
		err(1, "Memory allocation failed");
	}
	redirection redir = { NULL, NULL, 0 };
	int i;

	shell.var_error = 0;
	int pid, pidwait;
	int status;
	command cmd;

	// The program ends when EOF (ctrol+D)
	while (69) {
		printf("\033[1;92m%s\033[0m", getcwd(pwd, sizeof(pwd)));
		printf("\033[1;36m sharam> \033[0m");
		if (fgets(line, MAXLINE, stdin) != NULL) {
			// Passing them by value so to modify the context
			lineParser(line, &shell, &redir);
		} else {
			// EOF detected (end of script file)
			break;	// Exit the loop and terminate the shell
			// It will then free all resources
		}
		for (i = 0; i < shell.arg_count; i++) {
			printf("arg[%d]: %s\n", i, shell.arguments[i]);
		}
		// This means no error during parse and hence is theoretically a valid command
		if (!shell.var_error) {
			cmd = lookCommand(shell.arguments[0]);
			if (shell.arg_count > 1
			    && strcmp(shell.arguments[shell.arg_count - 1],
				      "&") == 0) {
				cmd.background = 1;
				// arg_count--;
				free(shell.arguments[shell.arg_count - 1]);
				shell.arguments[shell.arg_count - 1] = NULL;	// To not pass it as argument
			} else
				cmd.background = 0;

			switch (cmd.type) {
			case -1:
				printf("Command not found\n");
				break;
			case 0:
				printf("Builtin found\n");
				switch (cmd.builtinIndex) {
				case 0:
					// Changing directory
					if (shell.arg_count == 2) {
						if (chdir(shell.arguments[1]) !=
						    0) {
							warnx
							    ("Could not change directory to %s",
							     shell.arguments
							     [1]);
							// reset errno
							errno = 0;
						}
					} else {
						printf
						    ("error: missing argument for cd\n");
					}
					break;
				default:
					break;
				}
				break;
			case 1:
			case 2:
				switch (pid = fork()) {
				case -1:
					warnx("Couldn't fork");
					break;
				case 0:
					// Input redirection
					if (redir.inputFile != NULL) {
						int fd = open(redir.inputFile,
							      O_RDONLY);

						if (fd < 0) {
							warnx
							    ("Could not open input file %s",
							     redir.inputFile);
						}
						dup2(fd, STDIN_FILENO);
						close(fd);
					}
					// Output redirection
					if (redir.outputFile != NULL) {
						int flags = O_WRONLY | O_CREAT;

						flags |=
						    (redir.appendMode ? O_APPEND
						     : O_TRUNC);
						int fd = open(redir.outputFile,
							      flags, 0644);

						if (fd < 0) {
							warnx
							    ("Could not open output file %s",
							     redir.outputFile);
						}
						dup2(fd, STDOUT_FILENO);
						close(fd);
					}
					execv(cmd.path, shell.arguments);
					warnx("Could not execute command");
					_exit(1);	// Exit child process
					break;
				default:
					printf("Path: %s, Type:%d\n", cmd.path,
					       cmd.type);
					if (!cmd.background) {
						while ((pidwait =
							wait(&status)) != -1) {
							if (pidwait == pid) {
								break;
							}
							// else it means other process finished
							continue;	// Simply wait
						}
					}
					break;
				}
				break;
			default:
				printf("Unknown command\n");
				break;
			}
			// Freeing and resetting to null redirecitons (in parent process)
			if (redir.inputFile != NULL) {
				free(redir.inputFile);
				redir.inputFile = NULL;
			}
			if (redir.outputFile != NULL) {
				free(redir.outputFile);
				redir.outputFile = NULL;
			}

			// We need to free path string
			if (cmd.path != NULL) {
				free(cmd.path);
			}
		}

		// Then we free all arguments
		for (i = 0; i < shell.arg_count; i++) {
			free(shell.arguments[i]);
			shell.arguments[i] = NULL;	// To mark as free
		}
		shell.arg_count = 0;
		shell.var_error = 0;
	}
	for (i = 0; i < shell.local_var_count; i++) {
		free(shell.local_var[i].name);
		free(shell.local_var[i].value);
	}
	free(shell.local_var);

	free(shell.arguments);

	exit(EXIT_SUCCESS);
}

command
lookCommand(char *cmd_name)
{
	command cmd_guess;

	cmd_guess = lookBuiltin(cmd_name);
	if (cmd_guess.type == -1) {
		// We have to look for the path
		cmd_guess = lookCurDir(cmd_name);
		if (cmd_guess.type == -1) {
			cmd_guess = lookPath(cmd_name);
		}
	}
	// else
	return cmd_guess;
}

command
lookBuiltin(char *cmd)
{
	int i;

	char *builtins[] = {
		"cd",
		"ifok",
		"ifnot"
	};

	for (i = 0; i < sizeof(builtins) / sizeof(char *); i++) {
		if (strcmp(cmd, builtins[i]) == 0)
			return (command) {
			.type = 0,.builtinIndex = i};
	}

	return (command) {
	.type = -1};
}

command
lookCurDir(const char *path)
{
	char pwd[PATH_MAX];

	getcwd(pwd, sizeof(pwd));
	DIR *dir = opendir(pwd);
	struct dirent *entry;
	char fullpath[PATH_MAX];

	if (dir == NULL) {
		errno = 0;	// Reset errno
		return (command) {
		.type = -1};
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, path) == 0) {
			closedir(dir);
			int len =
			    snprintf(fullpath, sizeof(fullpath), "%s/%s", pwd,
				     path);

			if (len >= sizeof(fullpath)) {	// Path overflowed
				return (command) {
				.type = -1};
			}

			return (command) {
			.type = 1,.path = strdup(fullpath)};
		}
	}

	closedir(dir);
	return (command) {
	.type = -1};
}

command
lookPath(const char *path)
{
	char *env = getenv("PATH");

	// Needs to be duplicated since strtok_r modifies the string
	// And getenv returns a pointer to the environment variable itself
	char *line = strdup(env);
	char *dirPath, *saveptr;
	DIR *currentDir;
	struct dirent *entry;
	char fullpath[PATH_MAX];

	for (dirPath = strtok_r(line, ":", &saveptr); dirPath != NULL;
	     dirPath = strtok_r(NULL, ":", &saveptr)) {
		currentDir = opendir(dirPath);
		if (currentDir == NULL) {
			continue;
		}
		while ((entry = readdir(currentDir)) != NULL) {
			if (strcmp(entry->d_name, path) == 0) {
				closedir(currentDir);
				int len = snprintf(fullpath, sizeof(fullpath),
						   "%s/%s", dirPath, path);

				if (len >= sizeof(fullpath)) {	// Path overflowed
					return (command) {
					.type = -1};
				}
				free(line);
				// Path variable needs to be freed
				// Note that we need to duplicate to return a pointer
				// Since returning local gives memory problems (returning destroyed local data)
				return (command) {
				.type = 2,.path = strdup(fullpath)};
			}
		}
		closedir(currentDir);
	}

	free(line);
	// Verifying it found something
	return (command) {
	.type = -1};
}

int
handleVariableSubstitution(char *token, shellState *shell)
{
	char *temp = token + 1;
	int n;

	for (n = 0; n < shell->local_var_count; n++) {
		if (strcmp(shell->local_var[n].name, temp) == 0) {
			// If not im returning same as local_var and that gives memory/free problems
			shell->arguments[shell->arg_count++] =
			    strdup(shell->local_var[n].value);
			break;
		}
	}
	if (n == shell->local_var_count) {	// This means nothing was found
		printf("error: var %s does not exist\n", token + 1);
		shell->var_error = 1;
		return 0;
	}
	return 1;
}

int
parseRedirection(char *redirect_pos, char **saveptr, shellState *shell,
		 redirection *redir, int mode)
{
	/*
	   Mode 0 -> Input redirection
	   Mode 1 -> Output redirection
	 */
	char *token;
	char *redirToken;
	char saved;

	if (redirect_pos[1] == '\0') {
		// Format: "> " - need next token as filename
		token = strtok_r(NULL, " ", saveptr);
		if (token != NULL) {
			//&& !mode
			//&& mode -> alt syntax
			if (redir->inputFile != NULL && mode == 0) {
				free(redir->inputFile);
			} else if (redir->outputFile != NULL && mode == 1) {
				free(redir->outputFile);
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
			printf("error: missing file for redirection\n");
			shell->var_error = 1;
			// break;
			return 0;
		}
	} else {
		// Format: ">file" - filename is in this token
		// Verifying there's not multiple single chaining redirections in one string
		// In that case it would priotize last redirection (like in bash)
		// Although it wont create files for previous redirections (they are simply ignored)
		if (redir->inputFile != NULL && mode == 0) {
			free(redir->inputFile);
		} else if (redir->outputFile != NULL && mode == 1) {
			free(redir->outputFile);
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
redirectionHandler(char *token, char **saveptr, char *redirect_pos,
		   shellState *shell, redirection *redir)
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
		if (!parseRedirection(redirect_pos, saveptr, shell, redir, 0)) {
			// This means error in redirection
			return 0;
		}
	} else {
		if (redirect_pos[1] == '>') {
			redir->appendMode = 1;
			if (!parseRedirection
			    (redirect_pos + 1, saveptr, shell, redir, 1)) {
				// This means error in redirection
				return 0;
			}
		} else {
			redir->appendMode = 0;
			if (!parseRedirection
			    (redirect_pos, saveptr, shell, redir, 1)) {
				// This means error in redirection
				return 0;
			}
		}
	}
	// Succesfully redirected
	return 1;
}

void
variableAssigner(char *token, shellState *shell)
{
	char *varToken, *saveptr;
	int a, n;

	for (a = 0, varToken = strtok_r(token, "=", &saveptr); a < 2;
	     varToken = strtok_r(NULL, "=", &saveptr), a++) {
		if (varToken == NULL
		    || (strcmp(varToken, shell->arguments[0]) == 0)) {
			break;
		} else if (a) {
			// We are in the value
			printf("Value: %s\n", varToken);
			if (shell->local_var_count >= shell->local_var_capacity) {
				shell->local_var_capacity *= 2;	// Duplicate capacity
				shell->local_var =
				    realloc(shell->local_var,
					    shell->local_var_capacity *
					    sizeof(char *));
				if (!shell->local_var) {
					warnx("Memory allocation failed");
					shell->var_error = 1;
					break;
				}
			}
			// Duplicate varToken since we will loose reference once clearing arguments
			if (n == shell->local_var_count) {
				printf("Local_var_count: %d\n",
				       shell->local_var_count);
				shell->local_var[shell->local_var_count++]
				    .value = strdup(varToken);
			} else {
				printf("n: %d\n", n);
				free(shell->local_var[n].value);
				shell->local_var[n].value = strdup(varToken);
			}
		} else {
			printf("Var_name: %s\n", varToken);
			for (n = 0; n < shell->local_var_count; n++) {
				if (strcmp(shell->local_var[n].name,
					   varToken) == 0) {
					// If not im storing it twice, but the later will never be used
					break;
				}
			}
			if (n == shell->local_var_count) {
				shell->local_var[shell->local_var_count].name =
				    strdup(varToken);
			}
		}
	}
}

void
lineParser(char *line, shellState *shell, redirection *redir)
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
	for (i = 0, token = strtok_r(line, "  \t", &saveptr); token != NULL;
	     token = strtok_r(NULL, "  \t", &saveptr), i++) {
		if (shell->arg_count >= shell->arg_capacity) {
			shell->arg_capacity *= 2;	// Duplicate capacity
			shell->arguments =
			    realloc(shell->arguments,
				    shell->arg_capacity * sizeof(char *));
			if (!shell->arguments) {
				warnx("Memory allocation failed");
				shell->var_error = 1;
				break;
			}
			// The extra memory from realloc is garbage
			// So we need to manually set it to Null
			for (n = shell->arg_count; n < shell->arg_capacity; n++) {
				shell->arguments[n] = NULL;
			}
		}
		printf("token: %s\n", token);
		if (token[0] == '$') {
			if (handleVariableSubstitution(token, shell)) {
				// Go for next token
				continue;
			} else {	// This means nothing was found
				break;
			}
		}
		// Handling redirection operators
		else {
			redirect_pos = NULL;

			if ((redirect_pos = strpbrk(token, "><")) != NULL) {
				if (!redirectionHandler
				    (token, &saveptr, redirect_pos, shell,
				     redir)) {
					// This means error in redirection
					break;
				}
			} else {
				// Else we simply save the token as argument
				shell->arguments[shell->arg_count++] =
				    strdup(token);
			}
		}

		if (i == 0) {
			variableAssigner(token, shell);
		}
	}
}
