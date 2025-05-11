#include "sharam.h"

int
change_dir(ShellState *shell)
{
	// Changing directory
	if (shell->arg_count == 2) {
		if (chdir(shell->arguments[1]) != 0) {
			warnx("Could not change directory to %s",
			      shell->arguments[1]);
			// reset errno
			errno = 0;
			return -1;
		}
	} else {
		if (chdir(getenv("HOME")) != 0) {
			warnx("Could not change directory to %s",
			      getenv("HOME"));
			// reset errno
			errno = 0;
			return -1;
		}
	}
	return 0;
}

int
ifok(Command *cmd, ShellState *shell, Redirection *redir)
{
	// First we clean up any existing command resources and verify there is a second argument
	cleanup_command(cmd);
	if (shell->arg_count < 2) {
		warnx("ifok: missing command to execute");
		return -1;
	}
	// Creating new command from the second argument
	*cmd = new_command(shell->arguments[1]);	// Since 0 is ifok token

	if (cmd->type == NOTFOUND) {
		printf("sharam: %s: Command not found\n", shell->arguments[1]);
		return -1;
	}
	// Saving original arguments and point to subcommand arguments (to not pass ifok token)
	char **args = shell->arguments;

	shell->arguments = &args[1];

	int result = execute_command(*cmd, shell, redir);

	cleanup_command(cmd);
	shell->arguments = args;

	return result;
}
