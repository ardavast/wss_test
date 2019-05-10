all: get_idle set_idle memhog

get_idle:
	$(CC) -Wall -Wextra -o get_idle get_idle.c

set_idle:
	$(CC) -Wall -Wextra -o set_idle get_idle.c

memhog:
	$(CC) -Wall -Wextra -o memhog memhog.c
