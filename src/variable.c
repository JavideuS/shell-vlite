#include "sharam.h"

int
variable_assigner(char *token, ShellState *shell)
{
	char *varToken, *saveptr;
	int n = 0;		// Initializing n to avoid uninitialized use
	char *var_name = NULL;
	char *var_value = NULL;

	// First we extract the name and value
	varToken = strtok_r(token, "=", &saveptr);
	if (varToken == NULL || (strcmp(varToken, shell->arguments[0]) == 0)) {
		return 0;	// Not a variable assignment
	}
	// Then we save the variable name
	var_name = strdup(varToken);
	if (!var_name) {
		warnx("Memory allocation failed");
		shell->var_error = 1;
		return -1;
	}
	// Getting the value part
	varToken = strtok_r(NULL, "=", &saveptr);
	if (varToken == NULL) {
		free(var_name);
		return 0;	// No value, not a valid assignment
	}
	// Saving the variable value
	var_value = strdup(varToken);
	if (!var_value) {
		free(var_name);
		warnx("Memory allocation failed");
		shell->var_error = 1;
		return -1;
	}
	// First we check if it's the result variable (can't be changed)
	if (strcmp(var_name, shell->local_var[0].name) == 0) {
		warnx("error: result variable can't be changed");
		free(var_name);
		free(var_value);
		return -1;
	}
	// Checking if variable already exists
	for (n = 0; n < shell->local_var_count; n++) {
		if (shell->local_var[n].name
		    && strcmp(shell->local_var[n].name, var_name) == 0) {
			// Update existing variable
			free(shell->local_var[n].value);
			shell->local_var[n].value = var_value;
			free(var_name);	// We don't need this since we're using existing name
			return 1;
		}
	}

	// It's a new variable, checking if we need to expand the array
	if (shell->local_var_count >= shell->local_var_capacity) {
		int new_capacity = shell->local_var_capacity * 2;
		LocalVariables *new_vars = realloc(shell->local_var,
						   new_capacity *
						   sizeof(LocalVariables));
		if (!new_vars) {
			free(var_name);
			free(var_value);
			warnx("Memory allocation failed");
			shell->var_error = 1;
			return -1;
		}
		// Initialize the new memory to prevent accessing garbage
		for (int i = shell->local_var_count; i < new_capacity; i++) {
			new_vars[i].name = NULL;
			new_vars[i].value = NULL;
		}

		shell->local_var = new_vars;
		shell->local_var_capacity = new_capacity;
	}
	// Adding the new variable
	shell->local_var[shell->local_var_count].name = var_name;
	shell->local_var[shell->local_var_count].value = var_value;
	shell->local_var_count++;

	return 1;
}
