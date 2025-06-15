myCLibrary.dll: myCLibrary.c
	gcc -shared -o myCLibrary.dll myCLibrary.c -lws2_32