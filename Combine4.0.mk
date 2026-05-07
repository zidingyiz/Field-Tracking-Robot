CC=c51
# 1. ADDED scanner.obj TO THE LIST HERE:
OBJS= main.obj motor.obj auto_mode.obj collision.obj vl53l0x.obj joystick.obj buzzer.obj scanner.obj

# The default 'target' (output) is Blinky.hex and 'depends' on
# the object files listed in the 'OBJS' assignment above.
# These object files are linked together to create Blinky.hex.
main.hex: $(OBJS)
	$(CC) $(OBJS)
	@echo Done!

# 2. ADDED scanner.h TO THE MAIN DEPENDENCIES:
main.obj: main.c auto_mode.c vl53l0x.c variable.h motor.h auto_mode.h collision.h vl53l0x.h joystick.h scanner.h
	$(CC) -c main.c

motor.obj: motor.c auto_mode.c vl53l0x.c variable.h motor.h auto_mode.h collision.h vl53l0x.h
	$(CC) -c motor.c

auto_mode.obj: auto_mode.c variable.h vl53l0x.c motor.h auto_mode.h collision.h vl53l0x.h
	$(CC) -c auto_mode.c
	
collision.obj: collision.c auto_mode.c vl53l0x.c variable.h motor.h auto_mode.h collision.h vl53l0x.h
	$(CC) -c collision.c

vl53l0x.obj:  vl53l0x.c collision.c auto_mode.c variable.h motor.h auto_mode.h collision.h vl53l0x.h
	$(CC) -c vl53l0x.c
	
joystick.obj:  joystick.c collision.c auto_mode.c variable.h motor.h auto_mode.h collision.h vl53l0x.h joystick.h
	$(CC) -c joystick.c

buzzer.obj: buzzer.c collision.c vl53l0x.c variable.h collision.h vl53l0x.h buzzer.h
	$(CC) -c buzzer.c

# 3. ADDED THE NEW COMPILATION RULE FOR SCANNER:
scanner.obj: scanner.c scanner.h variable.h motor.h
	$(CC) -c scanner.c
	
# Target 'clean' is used to remove all object files and executables
# associated wit this project
clean:
	@del $(OBJS) *.asm *.lkr *.lst *.map *.hex *.map 2> nul

# Target 'FlashLoad' is used to load the hex file to the microcontroller 
# using the flash loader.  If the folder of the flash loader has been
# added to 'PATH' just 'EFM8_prog' is needed.  Otherwise, a valid path
# must be provided as shown below.
LoadFlash:
	EFM8_prog.exe -ft230 -r main.hex

# Phony targets can be added to show useful files in the file list of
# CrossIDE or execute arbitrary programs:
Dummy: main.hex main.Map
	
explorer:
	cmd /c start explorer .