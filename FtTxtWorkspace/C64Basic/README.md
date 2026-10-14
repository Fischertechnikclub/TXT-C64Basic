Once the C64Basic program is compiled you need to copy the executable Debug/C64Basic to the TXT.
I recommend WINSCP for this. If you did not compile the program locally you need to copy the executable from the repository to a local directory.
First connect the TXT to the local LAN following the instructions for the TXT, in the WLAN Setup screen you should find the ip address of the TXT.
Now start WINSCP and use New Tab to create a connection, for the Host name use the ip address of the TXT, for the User name use ROBOPro and for the Password also use ROBOPro.
Save the session with for instance the name of the TXT, like TXT-1234 and Login.
In the local (left) pane, navigate to the directory containing the executable, in the remote (right) pane, navigate to /opt/knobloch/C-Program.
Now drag the C64Basic file from the left to the right pane. Right click the file in the right pane and select Properties, make sure all X boxes (eXecute) are checked. The file will now be visible on the TXT File -> C-Program. You could start it from there but it will exit with a message that no terminal is connected.
The C64Basic needs a terminal for input and because the TXT has no keys we will use a remote terminal. From WINSCP use Commands -> Open in PuTTY. A terminal window opwns, the user name is already set but you have to enter the password ROBOPro again.
You are now in the Linux shell and you can navigate to the C64Basic with `cd C-Program`
