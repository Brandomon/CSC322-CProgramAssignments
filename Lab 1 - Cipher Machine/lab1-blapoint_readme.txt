Brandon LaPointe
blapoint@cs.oswego.edu
Lab 1 Read Me File

All functions, including encode and decode functions, are working.

My first iteration of seperating the input string into a command and text using strtok() would stop at the fist sign of ')'
so I made a version, not using strtok(), that takes in the entire text input up until it encounters ')' then '\0'.
This expanded the functionality of encrypt and decrypt to be able to work with inputs such as "encrypt(hello(world)(program)!)"

Char arrays are designed to be large enough to hold:
	-input[809] : 100 binary chars and decode command, including start '(', end ')', and '\0' terminal
	-text[801]  : 100 Binary Chars
	-command[8] : largest command

Encrypt and decrypt functions include characters ranging from ASCII 32 to ASCII 126 to exclude the command chars (0-31) and delete char (127).

Decode only takes in groups of 8 chars consisting of 1's and 0's.

All error messages will result with user needing to re-enter input and a recheck of all formatting rules.

Error messages include:
	-Format error for input not ending with ')' char.
	-Command error for command input not matching listed commands.
	-Binary error for text input consisting of an off number of "bit" chars.
	-Binary error for text input containing characters other than 1's and 0's.