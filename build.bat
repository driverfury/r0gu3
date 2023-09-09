@echo off

rem Build the project
cl src\main.c /Febuild\debug.exe -nologo -W4 -FC -Z7 -GS- -Gs99999 -link -incremental:no -opt:ref -nodefaultlib -entry:main kernel32.lib -stack:100000,100000
