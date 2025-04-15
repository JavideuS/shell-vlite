#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //For getcwd
#include <linux/limits.h> //For PATH_MAX
#include <errno.h> //For errno
#include <err.h>//For warnings/errors
#include <sys/wait.h> //For wait

#include <signal.h>//For waiting background processes

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
    int background;
} command;

typedef struct localVariables{
    char* name;
    char* value;
} localVariables;

command lookCommand(char*);
command lookBuiltin(char *);
command lookPath(const char*);
command lookCurDir(const char*);
void chilldHandler(int sig);

int main(int argc, char *argv[]) {
    char line[MAXLINE];
    char pwd[PATH_MAX];
    char* token, *varToken;
    char* saveptr, *saveptr2;
    int local_var_capacity = 5;
    int arg_capacity = 10; //Intial argument capacity will be 10, in case it isn't enough, it will realloc the double
    int arg_count = 0; //Note that C doesn't have double initialization
    int local_var_count = 0;
    localVariables* local_var = (localVariables*)malloc(local_var_capacity * sizeof(localVariables));
    char** arguments = calloc(arg_capacity,sizeof(char*));
    int i,j,n;
    int active = 1;
    int var_error = 0;
    int pid;
	int status;
    command cmd;
    signal(SIGCHLD,chilldHandler);

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
                if(token[0] == '$'){
                    char* temp = token + 1;
                    for(n=0; n < local_var_count; n++){
                        if(strcmp(local_var[n].name,temp) == 0){
                            //If not im returning same as local_var and that gives memory/free problems
                            arguments[i] = strdup(local_var[n].value); 
                            arg_count++;
                            break;
                        }
                    }
                    if(n == local_var_count){ //This means nothing was found
                        printf("error: var %s does not exist\n", token+1);
                        var_error = 1;
                        break;
                    }
                }
                else{
                    arguments[i] = strdup(token);
                    arg_count++;
                }
                //Then we seprate by equals symbols
                for(j=0,varToken = strtok_r(token,"=",&saveptr2); i == 0; varToken = strtok_r(NULL,"=",&saveptr2),j++){
                    if(varToken == NULL || (strcmp(varToken, arguments[i]) == 0)){
                        break;
                    }
                    else if(j){
                        //We are in the value
                        printf("Value: %s\n", varToken);
                        if(local_var_count >= local_var_capacity){
                            local_var_capacity *= 2; //Duplicate capacity
                            local_var = realloc(local_var, local_var_capacity * sizeof(char*));
                        }
                        //Duplicate varToken since we will loose reference once clearing arguments
                        local_var[local_var_count].value = strdup(varToken); 
                        local_var_count++;
                    }else{
                        printf("Var_name: %s\n", varToken);
                        local_var[local_var_count].name = strdup(varToken);
                    }
                }
            }
        }
        for(i=0; i < arg_count; i++){
            printf("arg[%d]: %s\n", i, arguments[i]);
        }
        //This means no var error
        if(!var_error){
            cmd = lookCommand(arguments[0]);
            if(arg_count > 1 && strcmp(arguments[arg_count-1],"&") == 0){
                cmd.background = 1;
                //arg_count--;
                free(arguments[arg_count-1]);
                arguments[arg_count-1] = NULL; //To not pass it as argument
            }
            else
                cmd.background = 0;

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
                case 2:
                    switch(pid = fork()){
                        case -1:
                            warnx("Couldn't fork");
                            break;
                        case 0:
                            execv(cmd.path,arguments);
                            warnx("Could not execute command");
                            break;
                        default:
                            printf("Path: %s, Type:%d\n", cmd.path, cmd.type);
                            if(!cmd.background){
                                waitpid(pid, &status, 0);  // Wait specifically for the foreground process
                                // while((pid = wait(&status)) != -1){
                                //     continue; //Simply wait
                                // }
                            }
                            break;
                            
                    }
                    break;
                default:
                    printf("Unknown command\n");
                    break;
            }

            //We need to free path string
            if (cmd.path != NULL) {
                free(cmd.path);
            }
        }
        
        //Then we free all arguments
        for(i=0; i < arg_count; i++){
            free(arguments[i]);
            arguments[i] = NULL; //To mark as free
        }
        arg_count = 0;    
        var_error = 0;    

    }
    for(i=0; i < local_var_count; i++){
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
    char fullpath[PATH_MAX];

    if(dir == NULL){
        errno = 0; //Reset errno
        return (command){.type = -1};
    }

    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, path) == 0){
            closedir(dir);
            int len = snprintf(fullpath, sizeof(fullpath), "%s/%s", pwd, path);
            if (len >= sizeof(fullpath)) { //Path overflowed
                return (command){.type = -1};
            }

            return (command){.type = 1, .path = strdup(fullpath)};
        }
    }

    closedir(dir);
    return (command){.type = -1};
}

command lookPath(const char* path){
	char* env = getenv("PATH");
    //Needs to be duplicated since strtok_r modifies the string
    //And getenv returns a pointer to the environment variable itself
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
                int len = snprintf(fullpath, sizeof(fullpath), "%s/%s", dirPath, path);
                if (len >= sizeof(fullpath)) { //Path overflowed
                    return (command){.type = -1};
                }
                free(line);
                //Path variable needs to be freed
                //Note that we need to duplicate to return a pointer
                //Since returning local gives memory problems (returning destroyed local data)
                return (command){.type = 2, .path = strdup(fullpath)};
            }
        }
        closedir(currentDir);
    }

    free(line);
	//Verifying it found something
	return (command){.type = -1};
}

void chilldHandler(int sig){
    int status;
    pid_t pid;
    
    // Reap all terminated children
    //Wait pid to no block the program
    //The while is  in the case many children exit an once abut system only sends one signal
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child %d terminated\n", pid);
    }
}