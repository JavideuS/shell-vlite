#include "sharam.h"

Command
new_command(char *cmd_name)
{
	Command cmd_guess;

	cmd_guess = look_builtin(cmd_name);
	if (cmd_guess.type == -1) {
		// We have to look for the path
		cmd_guess = look_cur_dir(cmd_name);
		if (cmd_guess.type == -1) {
			cmd_guess = look_path(cmd_name);
		}
	}
	return cmd_guess;
}

Command
look_builtin(char *cmd)
{
	int i;

	char *builtins[] = {
		"cd",
		"ifok",
		"ifnot"
	};

	for (i = 0; i < sizeof(builtins) / sizeof(char *); i++) {
		if (strcmp(cmd, builtins[i]) == 0)
			return (Command) {
			.type = BUILTIN,.builtinIndex = i,.path =
			    NULL,.background = 0};
	}

	return (Command) {
	.type = NOTFOUND,.builtinIndex = -1,.path = NULL,.background = 0};
}

Command
look_cur_dir(const char *path)
{
	char pwd[PATH_MAX];

	if (getcwd(pwd, sizeof(pwd)) == NULL) {
		// Can't get current directory
		return (Command) {
		.type = NOTFOUND,.path = NULL};
	}

	DIR *dir = opendir(pwd);
	struct dirent *entry;
	char fullpath[PATH_MAX];

	if (dir == NULL) {
		errno = 0;	// Reset errno
		return (Command) {
		.type = NOTFOUND,.path = NULL};
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, path) == 0) {
			closedir(dir);
			int len =
			    snprintf(fullpath, sizeof(fullpath), "%s/%s", pwd,
				     path);

			if (len >= sizeof(fullpath)) {	// Path overflowed
				return (Command) {
				.type = NOTFOUND,.path = NULL};
			}

			char *path_copy = strdup(fullpath);

			if (!path_copy) {
				return (Command) {
				.type = NOTFOUND,.path = NULL};
			}

			return (Command) {
			.type = CURDIR,.path = path_copy,.builtinIndex =
				    -1,.background = 0};
		}
	}

	closedir(dir);
	return (Command) {
	.type = NOTFOUND,.path = NULL};
}

Command
look_path(const char *path)
{
	char *env = getenv("PATH");

	if (!env) {
		return (Command) {
		.type = NOTFOUND,.path = NULL};
	}
	// Needs to be duplicated since strtok_r modifies the string
	// And getenv returns a pointer to the environment variable itself
	char *line = strdup(env);

	if (!line) {
		return (Command) {
		.type = NOTFOUND,.path = NULL};
	}

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
					free(line);
					return (Command) {
					.type = NOTFOUND,.path = NULL};
				}

				char *path_copy = strdup(fullpath);

				free(line);

				if (!path_copy) {
					return (Command) {
					.type = NOTFOUND,.path = NULL};
				}

				return (Command) {
				.type = PATH,.path =
					    path_copy,.builtinIndex =
					    -1,.background = 0};
			}
		}
		closedir(currentDir);
	}

	free(line);
	return (Command) {
	.type = NOTFOUND,.path = NULL};
}

int
execute_command(Command cmd, ShellState *shell, Redirection *redir)
{
	switch (cmd.type) {
	case NOTFOUND:
		printf("sharam: %s: Command not found\n", shell->arguments[0]);
		free(shell->local_var[0].value);
		shell->local_var[0].value = strdup("127");
		return -1;
	case BUILTIN:
		//printf("Builtin found\n"); //For debugging
		switch (cmd.builtinIndex) {
		case CD:
			return change_dir(shell);
			break;
		case IFOK:
			if (strcmp(shell->local_var[0].value, "0") == 0) {
				return ifok(&cmd, shell, redir);
			}
			break;
		case IFNOT:
			//      Since it is the opposite of ifok
			//      I decided to use the same function but with the opposite condition
			if (strcmp(shell->local_var[0].value, "0") != 0) {
				return ifok(&cmd, shell, redir);
			}
		default:
			break;
		}
		break;
	case CURDIR:
	case PATH:
		return fork_and_exec(cmd, shell, redir);
	default:
		printf("error: Unknown command type\n");
		return -1;
	}
	return 0;
}

int
fork_and_exec(Command cmd, ShellState *shell, Redirection *redir)
{
	int pid, pidwait;
	int status;
	int fd, flags;

	switch (pid = fork()) {
	case -1:
		warnx("Couldn't fork");
		if (shell->heredoc_active == ACTIVEHEREDOC) {
			close(shell->herePipes[0]);
			close(shell->herePipes[1]);
		}
		return -1;
	case 0:
		if (cmd.background) {
			// Redirecting std in to /dev/null
			fd = open("/dev/null", O_RDONLY);
			if (fd < 0) {
				warnx("Could not open /dev/null");
			}
			dup2(fd, STDIN_FILENO);
			//dup2(fd, STDOUT_FILENO);
			close(fd);
		}
		// Input Redirection
		if (redir->inputFile != NULL) {
			fd = open(redir->inputFile, O_RDONLY);

			if (fd < 0) {
				warnx("Could not open input file %s",
				      redir->inputFile);
			}
			dup2(fd, STDIN_FILENO);
			close(fd);
		}
		// Output Redirection
		if (redir->outputFile != NULL) {
			flags = O_WRONLY | O_CREAT;

			flags |= (redir->appendMode ? O_APPEND : O_TRUNC);
			fd = open(redir->outputFile, flags, 0644);

			if (fd < 0) {
				warnx("Could not open output file %s",
				      redir->outputFile);
			}
			dup2(fd, STDOUT_FILENO);
			close(fd);
		}

		if (shell->heredoc_active == ACTIVEHEREDOC) {
			//Redirect stdin from the pipe
			dup2(shell->herePipes[0], STDIN_FILENO);
			close(shell->herePipes[0]);
		}

		if (cmd.path == NULL) {
			_exit(127);
		}

		execv(cmd.path, shell->arguments);
		warnx("Could not execute Command");
		_exit(1);	// Exit child process
		break;
	default:
		//printf("Path: %s, Type:%d\n", cmd.path, cmd.type); //For debugging
		if (!cmd.background) {
			while ((pidwait = wait(&status)) != -1) {
				if (pidwait == pid) {
					if (WIFEXITED(status)) {
						// We can get the exit status
						free(shell->local_var[0].value);
						shell->local_var[0].value =
						    int_to_string(WEXITSTATUS
								  (status));
					} else if (WIFSIGNALED(status)) {
						// We can get the signal number
						free(shell->local_var[0].value);
						shell->local_var[0].value =
						    int_to_string(WTERMSIG
								  (status));
					}
					break;
				}
				// else it means other process finished
				continue;	// Simply wait
			}
		}

		if (shell->heredoc_active == ACTIVEHEREDOC) {
			close(shell->herePipes[0]);

			//Setting heredoc_active to not active after reading is done
			shell->heredoc_active = ACTIVEHEREDOC - 1;
		}
		break;
	}
	return 0;
}

void
cleanup_command(Command *cmd)
{
	if (cmd->path != NULL) {
		free(cmd->path);
		cmd->path = NULL;
	}
}
