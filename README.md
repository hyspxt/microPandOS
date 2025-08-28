# µPandOS

## Description 
µPandOS is a simple, yet complete, educational OS built on three astraction phases:
- Level 1 - Queue Manager and fundamental data structures, such as *PCB* and *messages*.
- Level 2 - Nucleus (kernel).
- Level 3 - Support for user processes and virtual memory handling.

## Features 

- Round-robin scheduler - 5ms time slice.
- Exception handling.
- System Service Interface to create and terminate processes.
- Interrupt handling.
- Support 

## Usage

1. First of all, make sure you have installed the emulator. For example, if you are using Ubuntu, this is done by launching:
    ```sh
    sudo apt install umps3
    ```
    Instructions for others distro can be found directly in [µMPS3](https://github.com/virtualsquare/umps3/tree/master?tab=readme-ov-file#how-to-install) repository.

2. Compile project files, to create the `*.o` object files, by launching:
    ```sh
    cd microPandOS
    make
    ```
3. If you need to delete such object files and previous configuration of the emulator, you should use:
    ```sh
    make clean
    ``` 
4. Specifically for tests described in Level 3, you can generate testers with:
    ```sh
    make maketest
    ```
5. Symmetrically, to delete testers obj file:
    ```sh
    make cleantest
    ``` 

There is also a config example of the simulation machine provided: (```umps3.json```), which is the one I used for the exam. You can find more information about the whole development process in [relazione.pdf](https://github.com/hyspxt/microPandOS/blob/main/%CE%BCPandOS.pdf) (_in italian!_).

---

> cheers, hys 🎴
