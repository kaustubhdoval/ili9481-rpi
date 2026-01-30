Trynna make ILI9481 parallel TFT work with Rpi Zero 2W

- The display is just a generic arduino shield
- ILI9481 Datasheet (https://www.displayfuture.com/Display/datasheet/controller/ILI9481.pdf)

To Run

```
gcc ili9481_parallel.c -o ili9481_parallel -lgpiod
sudo ./ili9481_parallel
```
