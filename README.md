# LG-bootcamp-girin-gurim

Compile network server & device control modules

## HOW TO USE

- cd Network
- rm -rf server_app
- make
- copy server_app file to ubuntu or server computer.
- ./server_app

### Use kernel Image in Image directory

- kerenel should be v6.12.35
- Kerenal directory in gpio/kernel/Makefile should be setted correctly.
- At target board, "insmod device_Control.ko"
