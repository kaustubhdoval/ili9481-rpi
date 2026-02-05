Trynna make ILI9481 parallel TFT work with Rpi Zero 2W

- The display is just a generic arduino shield
- ILI9481 Datasheet (https://www.displayfuture.com/Display/datasheet/controller/ILI9481.pdf)

## To Run

```bash
gcc ili9481_parallel.c -o ili9481_parallel -lgpiod
sudo ./ili9481_parallel
```

## To compile on Windows WSL (Ubuntu based)

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

compile without checking libs (not reccomended)

```bash
arm-linux-gnueabihf-gcc \
  -DCOMPILE_CHECK \
  -fsyntax-only \
  -Wall -Wextra -Werror -pedantic \
  -I./mocks \
  ili9481_parallel.c
```
