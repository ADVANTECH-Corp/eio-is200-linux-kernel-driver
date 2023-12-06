## EIO-IS200 Linux Kernel Driver
> This is a series of device drivers for Advantech EIO-IS200 embedded controller based on Linux kernel 6.5.0 as follow:
```
MFD core: eiois200_core
GPIO: gpio-eiois200
Hardware monitor: eiois200-hwmon
Fan: eiois200_fan
I2C: i2c-eiois200
Video backlight: eiois200_bl
Watchdog: eiois200_wdt
```
## Get the source first.
> Get it from Github repository with the following command in the Linux terminal.

```bash
git clone git@github.com:ADVANTECH-Corpcd/eio-is200-linux-kernel-driver.git
cd eio-is200-linux-kernel-driver
```
## Install
> Run the following commands in the Linux terminal.
```bash
  make                 # Build *.ko files
  sudo make install    # Install driver module
  sudo make load       # Load module
```
## Uninstall
> To uninstall the EIOIS200 driver, use the following "make" commands:
```bash
  sudo make unload     # Unload module
  sudo make uninstall  # Uninstall driver module
  make clean           # Clean *.ko files
```


## Example
> We have prepared an example to demonstrate how to use the EIOIS200 driver. Simply type "./example.sh" to execute it.
```bash
root# sudo ./example.sh
```
