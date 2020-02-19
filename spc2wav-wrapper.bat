@echo off

rem Change "--amp=" to whatver you prefer
rem If you want to be as accurate as possible,
rem use 256. This defaults to 512.

"%~dp0spc2wav.exe" --amp 512 %1
pause
