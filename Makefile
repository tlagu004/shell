# My Makefile for Shell.c

# Variables for Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g #Tells program to show all errors since I used perror, etc.
TARGET = osh
SOURCE = shell.c

#Init Build runs on 'make'
all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

# Clean the build directory
clean:
	rm -f $(TARGET)