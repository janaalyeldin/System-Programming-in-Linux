# System-Programming-in-Linux
# Unix Utilities in C

This repository contains custom implementations of basic Unix utilities written in C.

---


# Unix Utilities

A collection of custom Unix-like command line utilities implemented in C.

## Utilities Included

- `cp`: Copy files
- `mv`: Move (rename) files
- `myecho`: Echo arguments to standard output
- `mypwd`: Print the current working directory

---

## cp

`cp` is a utility to copy the contents of one file to another.

### Compilation

```bash
gcc -o mycp mycp.c
```

### Usage

```bash
./mycp source.txt target.txt
```

### Example

If `source.txt` contains:

```
Hello, Unix World!
```

Then after running the command:

```bash
./mycp source.txt copy.txt
```

`copy.txt` will also contain:

```
Hello, Unix World!
```

---

## mv

`mv` is a utility to move (rename) a file by copying its contents to a new file and deleting the original.

### Compilation

```bash
gcc -o mymv mymv.c
```

### Usage

```bash
./mymv oldname.txt newname.txt
```

### Example

Before running:

```bash
ls
```

```
oldname.txt
```

After running:

```bash
./mymv oldname.txt renamed.txt
```

Then:

```bash
ls
```

```
renamed.txt
```

---

## myecho

`myecho` prints all the arguments passed to it, separated by spaces.

### Compilation

```bash
gcc -o myecho myecho.c
```

### Usage

```bash
./myecho Hello world from custom echo!
```

### Example

```bash
./myecho Hello world from custom echo!
```

Output:

```
Hello world from custom echo!
```

---

## mypwd

`mypwd` prints the current working directory.

### Compilation

```bash
gcc -o mypwd mypwd.c
```

### Usage

```bash
./mypwd
```

### Example

```bash
./mypwd
```

Output:

```
Working directory: /home/user/projects/unix-utils
```

---

Let me know if you'd like to add more utilities or include features like error handling explanations, diagrams, or usage inside scripts!
