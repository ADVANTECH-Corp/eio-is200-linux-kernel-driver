## EIO-IS200 Linux Kernel Driver
> This is a series of device drivers for Advantech EIO-IS200 embedded controller based on Linux kernel 6.5.0

### How to Install / Uninstall EIOIS200 Driver
- To install the *.ko files, use the following "make" commands:
```bash
  root# make                 # Build *.ko files
  root# sudo make install    # Install driver module
  root# sudo make load       # Load module
```
- To uninstall the EIOIS200 driver, use the following "make" commands:
```bash
  root# make unload          # Unload module
  root# sudo make uninstall  # Uninstall driver module
  root# sudo make clean      # Clean *.ko files
```

### example.sh
- We have prepared an example to demonstrate how to use the EIOIS200 driver. Simply type "./example.sh" to execute it.
```bash
root# sudo ./example.sh
```
