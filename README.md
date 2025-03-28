# **Project Instructions**

This project was developed and tested on **Linux (Ubuntu 22.04.3 LTS)**.  
🚫 **Windows is not supported. Please run this program on Linux.**

---

## **   Compilation and Execution**

To compile and run the program, use the following commands:

```sh
gcc main.c -o main
./main
```

## **   Configuration File**

The config.txt file must follow this format:
```sh
instances: <number>
tanks: <number>
healers: <number>
dps: <number>
min time: <number>
max time: <number>
```

 Each variable, except for min time and max time, must be followed by a whitespace and a 32-bit positive integer.
 min time and max time must be 4-bit positive integers (values between 0 and 15).
 The maximum number of instances is automatically restricted by dividing the available free RAM by the stack size of a thread.
* The program will exit with an error if the input does not follow the abov restrictions.*
