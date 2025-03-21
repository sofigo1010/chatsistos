CC = gcc
CFLAGS = -Wall -I./include -I./src -I./src/utils -I./src/users -I./src/connections -I./src/threads
LIBS = -lwebsockets -lcjson -lpthread

SRC = \
  src/main.c \
  src/utils/logger.c \
  src/utils/time_utils.c \
  src/users/user_manager.c \
  src/connections/connection_manager.c \
  src/threads/thread_manager.c

OBJ = $(SRC:.c=.o)
TARGET = chat_server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
