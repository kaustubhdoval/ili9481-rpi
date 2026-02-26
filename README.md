Trynna make ILI9481 parallel TFT work with Rpi Zero 2W

- The display is just a generic arduino shield
- ILI9481 Datasheet (https://www.displayfuture.com/Display/datasheet/controller/ILI9481.pdf)

## To Run

cd into the ili9481-rpi directory and run `make` and then to run the main.c program

```bash
sudo ./main
```

## To compile ili9481_parallel.c on Windows WSL (Ubuntu based)

cd into correct windows directory

```bash
cd /mnt/c/Users/YourUsername/Desktop
```

full compile

```bash
arm-linux-gnueabihf-gcc \
  -Wall -Wextra -std=c11 \
  ili9481_parallel.c \
  -lgpiod \
  -o app
```

compile without checking libs (not recommended)

```bash
arm-linux-gnueabihf-gcc \
  -DCOMPILE_CHECK \
  -fsyntax-only \
  -Wall -Wextra -Werror -pedantic \
  -I./mocks \
  ili9481_parallel.c
```

## Compiling main.c

Cd into the correct directory and then:

```bash
arm-linux-gnueabihf-gcc \
  -DCOMPILE_CHECK \
  -fsyntax-only \
  -Wall -Wextra -Werror -pedantic \
  -I./mocks \
  main.c
```
