## EIO-IS200 Linux Kernel Driver
> This is a series of device drivers for Advantech EIO-IS200 embedded controller as follow:
```
  MFD core: eiois200_core
  GPIO: gpio-eiois200
  Hardware monitor: eiois200-hwmon
  Fan: eiois200_fan
  Thermal: eiois200_thermal
  I2C: i2c-eiois200
  Video backlight: eiois200_bl
  Watchdog: eiois200_wdt
```
## OS supported
> These drivers have been verified on:
 ```
    Ubuntu 20.04 and 22.04 with Linux kernel versions 5.15, 5.19, and 6.2.
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

## DKMS packaging for debian and derivatives
> DKMS is commonly used on debian and derivatives, like ubuntu, to streamline building extra kernel modules. If you need to package the source code into an installation package, please follow the instructions below. Please note that these instructions are based on version 0.0.2 of the source code. Before executing the commands, make sure to adjust the version number '0.0.2' according to the version you are currently using:
```bash
  sudo cp eiois200-0.0.2/ /usr/src/ -r
  sudo dkms add eiois200/0.0.2
  sudo dkms build eiois200/0.0.2
  sudo dkms mkdeb eiois200/0.0.2
```
> After all, the installation package should be placed in /var/lib/dkms/eiois200/0.0.2/dev/
> To verify this installation package, please follow the instructions below to test:
```bash
  cp /var/lib/dkms/eiois200/0.0.2/deb/eiois200-dkms_0.0.2_amd64.deb .
  sudo dpkg -i eiois200-dkms_0.0.2_amd64.deb
```
> To test whether the installation is successful, you can test loading one of the modules, for example:
```bash
  sudo modprobe eiois200-hwmon
  lsmod | grep eio
```
> If correct, the reply should be as follows:
```bash
  eiois200_hwmon         40960  0
  eiois200_core          28672  1 eiois200_hwmon
```
> To uninstall the installation package, please follow the steps below:
```bash
  sudo dpkg -P eiois200-dkms
```
> To confirm that the package works properly, please use the following command:
```bash
  sudo bash /usr/src/eiois200-0.0.2/example.sh
```
> The shell script will start. You may use this sample to test each device driver:
```
**********************************************
**            EIOIS200 Example              **
**********************************************

Main (EIOIS200 driver demo script)

0) Terminate this program
1) Watch Dog
2) HWM
3) SmartFan
4) Thermal Protection
5) GPIO
6) Backlight
7) SMBus
8) I2C
9) Information

Enter your choice: 
```
## Reference
> #### GPIO sysfs: https://www.kernel.org/doc/Documentation/gpio/sysfs.txt
> #### hwmon sysfs: https://www.kernel.org/doc/Documentation/hwmon/sysfs-interface
> #### Thermal & Fan sysfs: https://www.kernel.org/doc/Documentation/thermal/sysfs-api.txt
> #### I2C sysfs: https://www.kernel.org/doc/Documentation/i2c/i2c-sysfs.rst
> #### Watchdog api: https://www.kernel.org/doc/Documentation/watchdog/watchdog-api.txt
