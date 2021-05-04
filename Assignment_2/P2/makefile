NAME=hyperquicksort_openmp

CC=gcc
# FLAGS=-Wall -Werror -Wextra
FLAGS=-Wall -Wextra -fopenmp -lm
DEBUG_FLAGS=-g -fsanitize=address
SRC=hyperquicksort_openmp.c
LIBS=
OBJ=
RM =rm -rf
ARR_COUNT=128 # Each thread gets 10 elements

all: $(NAME)

$(NAME): $(SRC)
	$(CC) $(FLAGS) $(LIBS) $(SRC) -o $(NAME)

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all
run:
	./$(NAME) $(ARR_COUNT)
rerun: re
	./$(NAME) $(ARR_COUNT)
