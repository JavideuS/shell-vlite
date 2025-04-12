#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //For getcwd
#include <linux/limits.h> //For PATH_MAX
#include <errno.h> //For errno

//To open folders
#include <sys/types.h>
#include <dirent.h>

enum {
    MAXLINE = 2048, //Retrieved from getconf LINE_MAX in my terminal
};

typedef struct command{
    int type;
    char* path;
    int builtinIndex;
} command;

typedef struct localVariables{
    char* name;
    char* value;
} localVariables;

command lookCommand(char*);
command lookBuiltin(char *);
command lookPath(const char*);
command lookCurDir(const char*);


int main(int argc, char *argv[]) {
    char line[MAXLINE];
    char pwd[PATH_MAX];
    char* token, *varToken;
    char* saveptr, *saveptr2;
    localVariables* local_var = (localVariables*)malloc(sizeof(localVariables));
    int local_var_size = 0;
    int arg_capacity = 10; //Intial argument capacity will be 10, in case it isn't enough, it will realloc the double
    int arg_count = 0;
    char** arguments = malloc(arg_capacity * sizeof(char*));
    int i,j;
    int active = 1;
    command cmd;

    while(active){
        printf("\033[1;92m%s\033[0m", getcwd(pwd, sizeof(pwd)));
        printf("\033[1;36m sharam> \033[0m");
        if(fgets(line,MAXLINE,stdin) != NULL){
            // Remove trailing newline
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') {
                line[len-1] = '\0';
}
            //First we separate by lines
            for(i=0,token = strtok_r(line, " ",&saveptr);token != NULL; token = strtok_r(NULL, " ",&saveptr),i++){
                if(i >= arg_capacity){
                    arg_capacity *= 2; //Duplicate capacity
                    arguments = realloc(arguments, arg_capacity * sizeof(char*));
                }
                printf("token: %s\n", token);
                arguments[i] = strdup(token);
                arg_count++;
                //Then we seprate by equals symbols
                for(j=0,varToken = strtok_r(token,"=",&saveptr2); varToken != NULL; varToken = strtok_r(NULL,"=",&saveptr2),j++){
                    break;
                    if(strcmp(varToken, arguments[i]) == 0){
                        continue;
                    }
                    else if(j){
                        //We are in the value
                        printf("Value: %s\n", varToken);
                        local_var[local_var_size].value = varToken;
                        local_var_size++;
                    }else{
                        printf("Var_name: %s\n", varToken);
                        local_var[local_var_size].name = varToken;
                    }
                }
            }
        }
        for(i=0; i < arg_count; i++){
            printf("arg[%d]: %s\n", i, arguments[i]);
        }
        cmd = lookCommand(arguments[0]);
        switch(cmd.type){
            case -1:
                printf("Command not found\n");
                break;
            case 0:
                printf("Builtin found\n");
                switch(cmd.builtinIndex){
                    case 0:
                        //Exiting program
                        active = 0;
                        break;
                    default:
                        break;
                }
                break;
            case 1:
                printf("Command found in current directory\n");
                break;
            case 2:
                printf("Command found in PATH: %s\n", cmd.path);
                break;
            default:
                printf("Unknown command\n");
                break;
        }
        //We need to free path string
        if (cmd.type == 2 && cmd.path != NULL) {
            free(cmd.path);
        }
        //Then we free all arguments
        for(i=0; i < arg_count; i++){
            free(arguments[i]);
            arguments[i] = NULL; //To mark as free
        }
        arg_count = 0;        

    }
    for(i=0; i < local_var_size; i++){
            free(local_var[i].name);
            free(local_var[i].value);
    }
    free(local_var);
    
    free(arguments);

    exit(EXIT_SUCCESS);
}

command lookCommand(char* cmd_name){
    command cmd_guess;
    cmd_guess = lookBuiltin(cmd_name);
    if(cmd_guess.type == -1){
        //We have to look for the path
        cmd_guess = lookCurDir(cmd_name);
        if(cmd_guess.type == -1){
            cmd_guess = lookPath(cmd_name);
        }
    }
    //else
    return cmd_guess;
}

command lookBuiltin(char *cmd) {
	int i;
	char* builtins[] = {
        "exit",
		"cd",
		"ifok",
		"ifnot"
	};

	for(i = 0; i < sizeof(builtins)/sizeof(char*); i++){
		if (strcmp(cmd, builtins[i]) == 0)
			return (command){.type = 0, .builtinIndex = i};
	}

	return  (command){.type = -1};
}

command lookCurDir(const char* path){
    char pwd[PATH_MAX];
    getcwd(pwd, sizeof(pwd));
    DIR* dir = opendir(pwd);
    struct dirent* entry;

    if(dir == NULL){
        errno = 0; //Reset errno
        return (command){.type = -1};
    }

    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, path) == 0){
            closedir(dir);
            return (command){.type = 1};
        }
    }

    return (command){.type = -1};
}

command lookPath(const char* path){
	char* env = getenv("PATH");
    char* line = strdup(env);
    char* dirPath, *saveptr;
    DIR* currentDir;
    struct dirent* entry;
    char fullpath[PATH_MAX];

    for(dirPath = strtok_r(line, ":",&saveptr);dirPath != NULL; dirPath = strtok_r(NULL, ":",&saveptr)){
        currentDir = opendir(dirPath);
        if(currentDir == NULL){
            continue;
        }
        while((entry = readdir(currentDir)) != NULL){
            if(strcmp(entry->d_name, path) == 0){
                closedir(currentDir);
                char* dirPathDup = strdup(dirPath);//When we free line dirPath loses it's reference
                //So we need to clone 
                free(line);
                snprintf(fullpath, sizeof(fullpath), "%s/%s", dirPathDup, path);
                //Path variable needs to be freed
                //Note that we need to duplicate to return a pointer
                //Since returning local gives memory problems (returning destroyed local data)
                return (command){.type = 2, .path = strdup(fullpath)};
            }
        }
        closedir(currentDir);
    }

	//Verifying it found something
	return (command){.type = -1};
}
