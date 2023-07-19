@echo off
setlocal
clang ./client/agent.c -o ./client/agent-c.exe -lws2_32 -g
