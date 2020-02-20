@echo off

rem Change "--amp=" to whatver you prefer
rem If you want to be as accurate as possible,
rem use 256. This defaults to 384.

"%~dp0spc2wav.exe" --amp 384 %1
pause
