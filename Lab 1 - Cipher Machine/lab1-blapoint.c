#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

int main()
{
    //Constants
    const char COMMANDENDCHAR[2] = "(";     //char used to signify end of command and start of text in formatted input
    const char TEXTENDCHAR[2] = ")";        //char used to signify end of text in formatted input
    const char ENCRYPT[8] = "encrypt";      //char array used for match comparison using strcmp() to encrypt
    const char DECRYPT[8] = "decrypt";      //char array used for match comparison using strcmp() to decrypt
    const char ENCODE[7] = "encode";        //char array used for match comparison using strcmp() to encode
    const char DECODE[7] = "decode";        //char array used for match comparison using strcmp() to decode
    const char EXIT[5] = "exit";            //char array used for match comparison using strcmp() to exit
    const int  MATCH = 0;                   //constant int for match comparison using strcmp()
    const int  KEY = 5;                     //int value of shift key for Ceasar's Cipher

    //Variables
    bool flag;                              //boolean flag used to signify proper formatting and input
    bool decodeFlag;                        //boolean flag used to signify proper formatting of decode command text input
    char input[809];                        //char array for user input with enough space for the command and up to 100 bytes/binary-chars
    char command[8];                        //char array for user command input with enough space for largest command
    char text[801];                         //char array for user text input with enough space to decode 100 bytes/binary-chars
    char byte[9];                           //char array used for chunking binary into groups of 8 chars
    char ch;                                //char used to cycle through each character of char array
    int  stringEndValue;                    //int used to remove new line added by fgets()
    int  commandEndValue;                   //int used to separate command and input
    int  textEndValue;                      //int used to check if input ends with ")" textEndChar
    int  textLength;                        //int used to check if input contains chars other than 1 and 0
    int  compareValue;                      //int used to compare input to commands
    int  i;                                 //int used to count through each character of char array

    //Cipher Machine instruction output
    printf("\nWelcome to the Cipher Machine.\n");
    printf("Please enter using the format: command(Text Input) or exit.\n\n");
    printf("List of Commands:\n");
    printf("encrypt - Encrypt Text using Ceasar's Cipher\n");
    printf("decrypt - Decrypt Text using Ceasar's Cipher\n");
    printf("encode - Encode Text to Binary\n");
    printf("decode - Decode Binary to Text\n");
    printf("exit - Exit the Cipher Machine Program\n\n");

    do //Keep running until command is "exit"
    {
        do //Get input until formatted correctly, giving error messages when incorrect
        {
            //Set flag to true to show zero errors within input
            flag = true;

            //Get user input
            printf("blapoint $ ");
            fgets(input, 809, stdin);

            //Remove new line added by fgets() if present
            stringEndValue = strlen(input) - 1;
            if (input[stringEndValue] == '\n')
                input[stringEndValue] = '\0';

            //Set textEndValue to length of input string - 1 to check for ")" char at end of input
            textEndValue = strlen(input) - 1;

            //If ")" is not at end of input, display error message
            if(input[textEndValue] != ')' && strcmp(input, EXIT) != MATCH) 
            {
                //Display format error message
                printf("[ERROR] - Please format input correctly\n");

                //Set flag to false
                flag = false;
            }

            /*******************************************************************************
            //Seperate input string into command and text.
            //(First iteration) - Stops at the first sign of ')' character, limiting encode and decode functions
            command = strtok(input, COMMANDENDCHAR);
            text = strtok(NULL, TEXTENDCHAR);
            *******************************************************************************/
            
            //Seperate string to include extra ')' characters
            //Set commandEndValue to stringEndValue as a base
            commandEndValue = stringEndValue;

            //Seperate command from input
            //For each element of the character array until commandEndValue
            for(i = 0; i < commandEndValue + 1; i++)
            {
                //If input array at i is equal to '(', set command array at i to '\0'
                if(input[i] == '(')
                {
                    //Set command array at i to '\0'
                    command[i] = '\0';
                    //Set commandEndValue to i
                    commandEndValue = i;
                }
                else
                {
                    //Collect the command input
                    command[i] = input[i];
                }
            }

            //Seperate text from input
            //For each element of the input array from the end character of command + 1
            for(i = 1; i < stringEndValue - commandEndValue + 1; i++)
            {
                //If input char is ')' followed by '\0' end text
                if(input[i + commandEndValue + 0] == ')' && input[i + commandEndValue + 1] == '\0')
                {
                    //Set text array at i - 1 to '\0'
                    text[i - 1] = '\0';
                }
                else // Else set text array at i - 1 to input array at i + commandEndValue
                {
                    //Set text array at i - 1 to input of i + commandEndValue
                    text[i - 1] = input[i + commandEndValue];
                }
            }

            //If command doesn't match listed commands, display error message
            if(strcmp(command, ENCRYPT) !=  MATCH && 
               strcmp(command, DECRYPT) != MATCH && 
               strcmp(command, ENCODE) != MATCH && 
               strcmp(command, DECODE) != MATCH && 
               strcmp(command, EXIT) !=  MATCH)
            {
                //Display command error message
                printf("[ERROR] - Please enter one of the commands listed above.\n");
                
                //Set flag to false
                flag = false;
            }

            //Display error message if command is decode and input has an off number of binary digits
            if(strcmp(command, DECODE) == MATCH && strlen(text) % 8 != 0)
            {
                //Display binary digit off number input error
                printf("[ERROR] - Binary input consisted of an off number of bits.\n");

                //Set flag to false
                flag = false;
            }

            //Set text length to check if binary contains only 1 + 0
            textLength = strlen(text);

            //Display error message if command is decode and input contains characters other than 1 and 0
            if(strcmp(command, DECODE) == MATCH)
            {
                //Set decodeFlag to true before test
                decodeFlag = true;

                //Run through each char of text char array and set flag to false if any char is not a 1 or 0
                for(i = 0; i < textLength; i++)
                {
                    //If text input includes chars other than 1 and 0, set decodeFlag to false
                    if(text[i] != '1' && text[i] != '0')
                    {
                        //Set decodeFlag to false
                        decodeFlag = false;
                    }
                }

                //If decodeFlag is false, display error message and set flag to false
                if(decodeFlag == false)
                {
                    //Display decode input format error
                    printf("[ERROR] - Binary input should only contain 1's and 0's.\n");

                    //Set flag to false
                    flag = false;
                }

            }          

            printf("\n");
 
        }while(flag == false); //End DO WHILE (Get input until formatted correctly, giving error messages when incorrect)

        //If command = ENCRYPT
        if(strcmp(command, ENCRYPT) == MATCH)
        {
            printf("ENCRYPTING...\n");

            //For each element within the char array, encrypt
            for(i = 0; text[i] != '\0'; i++)
            {
                //If text array at i is greater than 'z' 
                if(text[i] > 'z')
                {
                    //Encrypt text array at i, subtracting '_' ('~' - ' ')
                    text[i] = text[i] - '_' + KEY;
                }

                else //Else encrypt text array at i
                {
                    //Encrypt text array at i
                    text[i] = text[i] + KEY;
                }
            }

            //Output encrypted text
            printf("%s\n\n", text);
        }

        //If command = DECRYPT
        if(strcmp(command, DECRYPT) == MATCH)
        {
            printf("DECRYPTING...\n");

            //For each element within the char array
            for(i=0; text[i] != '\0'; i++)
            {
                //Decrypt text array at i
                text[i] = text[i] - KEY;

                //If text array at i is less than ' ', compensate by adding '_' ('~' - ' ')
                if(text[i] < ' ')
                {
                    //Set text array at i to text array at i + '_'
                    text[i] = text[i] + '_';
                }

            }

            //Output decrypted text
            printf("%s\n\n", text);
        }

        //If command = ENCODE
        if(strcmp(command, ENCODE) == MATCH)
        {
            printf("ENCODING...\n");

            //For each character within the text array, convert to binary and display
            for(i = 0; i < 8 * strlen(text); i++)
            {
                //Convert to binary and display
                printf("%d",0 != (text[i/8] & 1 << (~i&7)));
            }

            printf("\n\n");
        }

        //If command = DECODE
        if(strcmp(command, DECODE) == MATCH)
        {
            printf("DECODING...\n");

            //For each chunk of 8 chars (binary digits) within the input, convert to char and display
            for(i = 0; i < strlen(text) / 8; i++)
            {
                //For each character within each chunk of 8, set byte array at count to text array at i * 8 + count
                for(int count = 0; count < 8; count++)
                {
                    //Set byte array at count to text array at i * 8 + count
                    byte[count] = text[i * 8 + count];
                }

                //Convert and display each chunk of 8 chars
                ch = strtol(byte, 0, 2);
                printf("%c", ch);
            }

            printf("\n\n");
        }

    }while(strcmp(input, EXIT) != MATCH); //End DO WHILE (Keep running until command is "exit") 

    //Diplay program close final output
    printf("Program Terminated...\n");

    return 0;

}//end main