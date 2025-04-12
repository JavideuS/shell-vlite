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
    char* duptoken;
    char* saveptr, *saveptr2;
    localVariables* local_var = (localVariables*)malloc(sizeof(localVariables));
    int local_var_size = 0;
    char** arguments = malloc(sizeof(char*));
    int i,j;
    command cmd;

    while(1){
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
                printf("token: %s\n", token);
                duptoken = strdup(token);
                arguments[i] = duptoken;
                //Then we seprate by equals symbols
                for(j=0,varToken = strtok_r(token,"=",&saveptr2); varToken != NULL; varToken = strtok_r(NULL,"=",&saveptr2),j++){
                    break;
                    if(strcmp(varToken, duptoken) == 0){
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
                //free(duptoken);
            }
        }
        for(i=0; arguments[i] != NULL; i++){
            printf("arg[%d]: %s\n", i, arguments[i]);
        }
        cmd = lookCommand(arguments[0]);
        switch(cmd.type){
            case -1:
                printf("Command not found\n");
                break;
            case 0:
                printf("Builtin found\n");
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

    }
    
    free(pwd);
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
		"cd",
		"ifok",
		"ifnot"
	};

	for(i = 0; i < sizeof(builtins)/sizeof(char*); i++){
		if (strcmp(cmd, builtins[i]) == 0)
			return (command){.type = 0, .builtinIndex = i+1};
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
                free(line);
                snprintf(fullpath, sizeof(fullpath), "%s/%s", dirPath, path);
                return (command){.type = 2, .path = fullpath};
            }
        }
        closedir(currentDir);
    }

	//Verifying it found something
	return (command){.type = -1};
}

