# Tetris in the terminal
The title says it all!

This project serves as an example in a [blog on making a terminal-rendered game](https://supsydemarciou.github.io/supsy.dev/blogs/tetris/index.html).

# How to compile
This project doesn't depend on any external library: 
```sh
gcc -o tetris tetris.c -lm
```

# How to use
Just launch the executable and the controls will be displayed. To start at a later level, just pass it via commad-line:
```sh
./tetris 18 # Start on level 18
```