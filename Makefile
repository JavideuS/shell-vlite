CC = gcc
CFLAGS = -Wall -Wshadow -Wvla -g -Iinclude

#Folders distribution
#src -> where all the c files are saved
#./src also works
SRC_DIR = src
#build -> where all the .o and generated files will be saved 
OBJ_DIR =  build
TARGET = sharam # Thats how I decided to call my shell lite

# #To look for every c file inside src/folder
SRCS = $(wildcard $(SRC_DIR)/*.c)
# #patsubst to change the .c to .o
# #pattern substitution text
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.RECIPEPREFIX = >

all: $(TARGET)

$(TARGET): $(OBJS)
#>@echo $(SRCS)
#>@echo $(OBJS)
>$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
>@mkdir -p $(OBJ_DIR) #In case the folder wasnt created
>$(CC) $(CFLAGS) -c $< -o $@

#This deletes every all the object files and the executable
clean:
>rm -rf $(OBJ_DIR) sharam

nothing:
>@echo "This command does nothing <3"