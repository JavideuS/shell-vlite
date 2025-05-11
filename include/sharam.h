#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>
#include <errno.h>
#include <err.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>

/* Constants */
enum {
	MAXLINE = 2048,
	ACTIVEHEREDOC = 1,
};

/* Enums */
enum builtin {
	CD = 0,
	IFOK,
	IFNOT,
};

enum commandType {
	BUILTIN = 0,
	CURDIR,
	PATH,
	NOTFOUND = -1
};

/* Data Structures */
typedef struct Command {
	int type;
	char *path;
	int builtinIndex;
	int background;
} Command;

typedef struct LocalVariables {
	char *name;
	char *value;
} LocalVariables;

typedef struct Redirection {
	char *inputFile;
	char *outputFile;
	char *errorFile;
	int appendMode;
} Redirection;

typedef struct ShellState {
	char **arguments;
	int arg_capacity;
	int arg_count;
	LocalVariables *local_var;
	int local_var_capacity;
	int local_var_count;
	int var_error;
	int heredoc_active;
	int herePipes[2];
} ShellState;

/* Function Declarations */

/* Command Functions */
Command new_command(char *cmd_name);
Command look_builtin(char *cmd);
Command look_path(const char *path);
Command look_cur_dir(const char *path);
int execute_command(Command cmd, ShellState * shell, Redirection * redir);
int fork_and_exec(Command cmd, ShellState * shell, Redirection * redir);
void cleanup_command(Command * cmd);

/* Parser Functions */
void line_parser(char *line, ShellState * shell, Redirection * redir);
int handle_variable_substitution(char *token, ShellState * shell);
int parse_redirection(char *redirect_pos, char **saveptr, ShellState * shell,
		      Redirection * redir, int mode);
int redirection_handler(char *token, char **saveptr, char *redirect_pos,
			ShellState * shell, Redirection * redir);
void check_here_doc(ShellState * shell, Redirection redir);

/* Builtin Functions */
int change_dir(ShellState * shell);
int ifok(Command * cmd, ShellState * shell, Redirection * redir);

/* Variable Functions */
int variable_assigner(char *token, ShellState * shell);

/* Shell Management Functions */
void initialize_shell(ShellState * shell);
void cleanup_shell(ShellState * shell);
void cleanup_redirection(Redirection * redir);
void cleanup_arguments(ShellState * shell);

/* Utility Functions */
char *int_to_string(int value);
