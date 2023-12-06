# Simon

This project is an implementation of the popular game Simon, for the RaspberryPi using a breadboard, some LEDs buttons.

## Driver

To implement the game, I decided to implement a custom Linux driver to control the LEDs and button using the built-in GPIO pins. The driver code is, as expected, in the `driver` directory.

### Building the driver

In order to build the driver you to install your kernel header files. Do so with the following command - 

```bash
sudo apt install -y kernel-headers-$(uname -r)
```

Then, to build the driver run the following commands - 
```bash
cd driver
make
```

Then you need to load the driver and the device tree overlay into the kernel. You can do so with the following commands

```bash
sudo insmod ./led.ko
sudo dtoverlay ledoverlay.dtbo
```

Now, your driver should be loaded into the kernel. You can verify it by running the command 

```bash
lsmod | grep led
```

You should see output like this 

```bash
led                    16384  0
```

A file should be created in the path `/proc/led`.

### Controlling the LEDs

To turn an LED on/off you need to write to the `/proc/led` file the index of the led and whether to turn it on or off. 

For example, if you wish to turn off LED 0 (which is connected to pin 17) run the command 

```bash
echo 0,1 > /proc/led
```

To turn off LED 3 (which is connected to ping 23) run the command

```bash
echo 3,0 > /proc/led
```

### Reading the buttons

In a similar manner to controlling the LEDs, in order to read the buttons you need to read the `/proc/led` file. Every time a button is pressed, its index is written to the file.

If you `cat /proc/led` you process will sleep until a button is pressed.
If you then press button 0, you should see `0` printed to the terminal.

## Game

The actual game logic is in the `game` directory.

### Building the game

In order to build the game run the following 

```bash
cd game
make
```

You can then start the game by running 

```bash
./simon
```

To start playing the game, enter 's', and follow the lights closely!

