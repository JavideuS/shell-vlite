CC = gcc
CFLAGS = -Wall -Wshadow -Wvla -g

.RECIPEPREFIX = >

all: sharam

sharam: sharam.o 
>$(CC) $(CFLAGS) -o sharam $^

sharam.o: sharam.c
>$(CC) $(CFLAGS) -c sharam.c

path: path.c
>$(CC) $(CFLAGS) -o path path.c