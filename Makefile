NAME=Matt_daemon

FLAG=-Wall -Werror -Wextra

CC=g++

SRC= ./srcs/main.cpp

LOG= /var/log/matt_daemon.log

LOCK= /var/lock/matt_daemon.lock

$(NAME):$(SRC)
	sudo $(CC) $(FLAG) $(SRC) -o $(NAME) 

all:$(NAME)

fclean:
	sudo rm -rf $(NAME)
	sudo rm -rf $(LOG)
	sudo rm -rf $(LOCK)

re:fclean all