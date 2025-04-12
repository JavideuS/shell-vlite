#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //For getcwd
#include <linux/limits.h> //For PATH_MAX

//TO open folders
#include <sys/types.h>
#include <dirent.h>
//strtok_r
//type 

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

command lookBuiltin(char *);


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

    while(1){
        printf("\033[1;92m%s\033[0m", getcwd(pwd, sizeof(pwd)));
        printf("\033[1;36m sharam> \033[0m");
        if(fgets(line,MAXLINE,stdin) != NULL){
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
            }
        }
        for(i=0; arguments[i] != NULL; i++){
            printf("arg[%d]: %s\n", i, arguments[i]);
        }

       
    }
    
    free(pwd);
    free(arguments);

    exit(EXIT_SUCCESS);
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


char* lookPath(const char* path){
	char* env = getenv("PATH");
	
	//Verifying it found something
	return env;
}