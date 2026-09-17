@echo off
::This file was created automatically by CrossIDE to compile with C51.
D:
cd "\elec 291(project2)\STM32\STM32L051 (1)\PrintADC\"
"D:\crossIDE\CrossIDE\Call51\Bin\c51.exe" --use-stdout  "D:\elec 291(project2)\STM32\STM32L051 (1)\PrintADC\main.c"
if not exist hex2mif.exe goto done
if exist main.ihx hex2mif main.ihx
if exist main.hex hex2mif main.hex
:done
echo done
echo Crosside_Action Set_Hex_File D:\elec 291(project2)\STM32\STM32L051 (1)\PrintADC\main.hex
