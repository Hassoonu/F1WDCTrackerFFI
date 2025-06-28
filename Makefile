myCLibrary.dll: myCLibrary.c
	gcc -shared -o myCLibrary.dll myCLibrary.c -I C:/msys64/clang64/include -L C:/msys64/clang64/lib -lws2_32 -lssl -lcrypto

run:
	python main.py