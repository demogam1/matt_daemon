NAME=Matt_daemon

FLAG=-Wall -Werror -Wextra

CC=g++

SRC= ./srcs/main.cpp

LOG= /var/log/matt_daemon.log

LOCK= /var/lock/matt_daemon.lock

$(NAME):$(SRC)
	$(CC) $(FLAG) $(SRC) -o $(NAME) 

all:$(NAME)

fclean:
	rm -rf $(NAME)
	rm -rf $(LOG)
	rm -rf $(LOCK)

re:fclean all