NAME=simon

all: game
	echo Built simon

game:
	gcc -o $(NAME) -I lib/ main.c lib/led.c

clean:
	rm -f $(NAME)

