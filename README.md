# System-Programming-in-Linux
# Unix Utilities in C

This repository contains custom implementations of basic Unix utilities written in C.

---

## 📂 mypwd

```c
// Compile:
gcc mypwd.c -o mypwd

// Run:
$ ./mypwd
/home/user/projects/unix-utils

## 📂 mycp
// Compile:
gcc mycp.c -o mycp

// Run:
$ cat file1.txt
Hello from the original file!

$ ./mycp file1.txt file2.txt

$ cat file2.txt
Hello from the original file!

## 📂 mymv
// Compile:
gcc mymv.c -o mymv

// Run:
$ ls
oldname.txt

$ ./mymv oldname.txt newname.txt

$ ls
newname.txt

## 📂 myecho
// Compile:
gcc myecho.c -o myecho

// Run:
$ ./myecho Hello Unix World!
Hello Unix World!

