# myCLibrary.dll: myCLibrary.c
# 	gcc -shared -o myCLibrary.dll myCLibrary.c -I C:/msys64/clang64/include -L C:/msys64/clang64/lib -lws2_32 -lssl -lcrypto

CC = gcc
PATH_TO_SRC = src/Backend_Logic


myCLibrary_linux.so: $(PATH_TO_SRC)/myCLibrary_linux.c
	$(CC) -fPIC -shared -o $(PATH_TO_SRC)/$@ $< -lssl -lcrypto

run:
	python src/Backend_Logic/main.py --log-level INFO