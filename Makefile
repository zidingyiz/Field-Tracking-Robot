CC = c51

SRC_DIR = src
INC_DIR = include

TARGET = main.hex
OBJS = main.obj motor.obj auto_mode.obj collision.obj vl53l0x.obj joystick.obj buzzer.obj scanner.obj

.PHONY: all clean LoadFlash Dummy explorer

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS)
	@echo Done!

main.obj: $(SRC_DIR)/main.c $(INC_DIR)/variable.h $(INC_DIR)/motor.h $(INC_DIR)/auto_mode.h $(INC_DIR)/collision.h $(INC_DIR)/vl53l0x.h $(INC_DIR)/joystick.h $(INC_DIR)/buzzer.h $(INC_DIR)/scanner.h
	$(CC) -I$(INC_DIR) -c $(SRC_DIR)/main.c

motor.obj: $(SRC_DIR)/motor.c $(INC_DIR)/variable.h $(INC_DIR)/motor.h
	$(CC) -I$(INC_DIR) -c $(SRC_DIR)/motor.c

auto_mode.obj: $(SRC_DIR)/auto_mode.c $(INC_DIR)/variable.h $(INC_DIR)/motor.h $(INC_DIR)/auto_mode.h $(INC_DIR)/collision.h $(INC_DIR)/vl53l0x.h
	$(CC) -I$(INC_DIR) -c $(SRC_DIR)/auto_mode.c

collision.obj: $(SRC_DIR)/collision.c $(INC_DIR)/variable.h $(INC_DIR)/collision.h $(INC_DIR)/vl53l0x.h
	$(CC) -I$(INC_DIR) -c $(SRC_DIR)/collision.c

vl53l0x.obj: $(SRC_DIR)/vl53l0x.c $(INC_DIR)/vl53l0x.h
	$(CC) -I$(INC_DIR) -c $(SRC_DIR)/vl53l0x.c

joystick.obj: $(SRC_DIR)/joystick.c $(INC_DIR)/variable.h $(INC_DIR)/motor.h $(INC_DIR)/joystick.h
	$(CC) -I$(INC_DIR) -c $(SRC_DIR)/joystick.c

buzzer.obj: $(SRC_DIR)/buzzer.c $(INC_DIR)/variable.h $(INC_DIR)/collision.h $(INC_DIR)/vl53l0x.h $(INC_DIR)/buzzer.h
	$(CC) -I$(INC_DIR) -c $(SRC_DIR)/buzzer.c

scanner.obj: $(SRC_DIR)/scanner.c $(INC_DIR)/variable.h $(INC_DIR)/motor.h $(INC_DIR)/scanner.h
	$(CC) -I$(INC_DIR) -c $(SRC_DIR)/scanner.c

clean:
	@del $(OBJS) *.asm *.lkr *.lst *.map *.hex *.map 2> nul

LoadFlash:
	EFM8_prog.exe -ft230 -r $(TARGET)

Dummy: $(TARGET) main.Map

explorer:
	cmd /c start explorer .
