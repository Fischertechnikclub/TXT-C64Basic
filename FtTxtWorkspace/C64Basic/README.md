# USING C64BASIC
## Preparations
Once the C64Basic program is compiled you need to copy the executable Debug/C64Basic to the TXT.
I recommend WINSCP for this. If you did not compile the program locally you need to copy the executable from the repository to a local directory.
First connect the TXT to the local LAN following the instructions for the TXT, in the WLAN Setup screen you should find the ip address of the TXT (Settings -> Network -> WLAN Setup -> Client).
Also make sure that the SSH daemon is running (Settings -> Security -> SSH Daemon).
Now start WINSCP and use New Tab to create a connection, for the Host name use the ip address of the TXT, for the User name use ROBOPro and for the Password also use ROBOPro.
Save the session with for instance the name of the TXT, like TXT-1234 and Login.
In the local (left) pane, navigate to the directory containing the executable, in the remote (right) pane, navigate to /opt/knobloch/C-Program.
Now drag the C64Basic file from the left to the right pane. Right click the file in the right pane and select Properties, make sure all X boxes (eXecute) are checked. The file will now be visible on the TXT File -> C-Program. You could start it from there but it will exit with a message that no terminal is connected.
The C64Basic needs a terminal for input and because the TXT has no keys we will use a remote terminal. From WINSCP use Commands -> Open in PuTTY. A terminal window opens, the user name is already set but you have to enter the password ROBOPro again.
## Running C64Basic
You are now in the Linux shell and you can navigate to the C64Basic with `cd C-Program`. Here you can start the program with `./C64Basic`. The LCD on the TXT will resemble the original Commodore 64 screen. Control-C will terminate the program and return to the Linux prompt, a few taps on the LCD screen will restore the main TXT screen. In the terminal you can enter C64 Basic commands or enter a program, eg.
```
10 FOR I=1 TO 10
20 PRINT I "HELLO"
30 NEXT I
```
Then type `RUN`
## Running fischertechnik programs
The fischertechnik programs were distributed on diskettes, these are barely available anymore and cannot be read by the TXT anyway. Some disk images are available however as *.d64 files. The C64Files program, also available in this repository, can extract the files from these images to a directory, this directory then functions as the diskdrive of the C64.
Suppose you have a disk image XXX.d64 in directory C-Program, you can extract these file to directory XXX with the command `./C64Files -a XXX.d64` then goto directory XXX with `cd XXX`. You can see the files with `ls`. Although the files often have the extension .BAS you cannot view them with the `cat` or `more` commands because they are in tokenized form (PRG files), each BASIC keyword is replaced with a non-ascii character and the character coding is PETSCII, not ASCII, which cannot be rendered by PuTTY. You can preview the files in the image with the C64Files utility. From the directory with the extracted files you can start C64Basic with `../C64Basic` or `/opt/knobloch/C-Program/C64Basic`.
You can see the directory from within C64Basic with `LOAD "$"`, load a program with `LOAD "PROGNAME.BAS"`, see the loaded program with `LIST` and execute it with `RUN`.

