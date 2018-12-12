# Drying_Hopper
A small scale hopper for plastic extruders that uses hot air to dry the plastic before and during extrusion.

Note: You must add Optiboot's board.txt definition for the Arduino Nano to the end of your boards.txt file to get access to Optiboot's bootloader. This bootloader must be burned to the board in order to use the watchdog timer. I had some errors and had to add a few lines to the Optiboot definition Here's what I added to boards.txt:

# Optiboot Arduino support
# http://optiboot.googlecode.com
# Peter Knight, 2010
# Bill Westfield, 2013 - now includes build.variant for 1.0.2 and later

##############################################################

atmega328o.name=[Optiboot] Arduino Duemilanove or Nano w/ ATmega328
atmega328o.upload.protocol=arduino
atmega328o.upload.tool=avrdude
atmega328o.upload.maximum_size=32256
atmega328o.upload.speed=115200
atmega328o.bootloader.low_fuses=0xff
atmega328o.bootloader.high_fuses=0xde
atmega328o.bootloader.extended_fuses=0xfd
atmega328o.bootloader.path=optiboot
atmega328o.bootloader.file=optiboot/optiboot_atmega328.hex
atmega328o.bootloader.unlock_bits=0x3F
atmega328o.bootloader.lock_bits=0x0F
atmega328o.bootloader.tool=avrdude
atmega328o.build.mcu=atmega328p
atmega328o.build.f_cpu=16000000L
atmega328o.build.core=arduino:arduino
atmega328o.build.variant=arduino:standard