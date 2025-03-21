CC = gcc
CFLAGS = -Wall -I./include -I./src/utils -I./src/users -I./src/connections
LIBS = -lwebsockets -lcjson

SRC = src/main.c src/utils/logger.c src/utils/time_utils.c src/users/user_manager.c src/connections/connection_manager.c
OBJ = $(SRC:.c=.o)
TARGET = chat_server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
