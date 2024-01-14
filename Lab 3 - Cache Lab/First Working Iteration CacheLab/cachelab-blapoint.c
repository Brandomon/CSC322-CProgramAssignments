// ****************************************************************************************************
//
//                                     cachelab-blapoint.c
//
// ****************************************************************************************************
//
//                                            Notes:
//
//                                  Memory Address Structure:
//
//                               [     tag     ][   s   ][  b  ]
//                                      |           |       |
//                                      |           |      (b) Block offset: b bits
//                                      |           |
//                                      |          (s) Set index of s bits -> Check for hit/miss
//                                      |
//                                    (tag) Tag Bits: (AddressSize - b - s)
//
//----------------------------------------------------------------------------------------------------
//
//                              Each cacheLine stores a block - Each block has B = 2^b bytes
//                              CacheSet is a set of E = 2^e cacheLines
//                              Cache is a set of S = 2^s cacheSets
//                              Total Capacity = S*B*E
//
//----------------------------------------------------------------------------------------------------
//
//                                              Cache Structure:
//
// CacheLine stores a block ---> [block]    
//      (B = 2^b bytes)                   [     ][     ][     ][     ][     ]    <-Set0
//        (B=4, b=2)               [     ][     ][     ][     ][     ][     ]    <-Set1
//                                 [     ][     ][     ][     ][     ][     ]    <-Set2
//                                 [     ][     ][     ][     ][     ][     ]    <-Set3 }-----CacheSet (E cachelines : E = 6)
//                                                      ^
//                                                     /|\ 
//                                                      |
//                                       Cache: (2^s cacheSets) (S=4, s=2)
//                                 -s (number of index bits) ->  2^s = numSets
//----------------------------------------------------------------------------------------------------
//
//                                      Format Specifiers:
//
//                           %c                  -       Char
//                           %d                  -       Signed Int
//                           %e or %E            -       Scientific Notation of Floats
//                           %f                  -       Float
//                           %hi                 -       Signed Int (short)
//                           %hu                 -       Unsigned Int (short)
//                           %i                  -       Unsigned Int
//                           %l or %ld or %li    -       Long
//                           %lf                 -       Double
//                           %Lf                 -       Long Double
//                           %lu                 -       Unsigned Int or Unsigned Long
//                           %lli or %lld        -       Long Long
//                           %llu                -       Unsigned Long Long
//                           %o                  -       Octal representation
//                           %p                  -       Pointer
//                           %s                  -       String
//                           %u                  -       Unsigned Int
//                           %x or %X            -       Hexadecimal representation
//                           %n                  -       Prints nothing
//                           %%                  -       Prints % character
//
//----------------------------------------------------------------------------------------------------
// ****************************************************************************************************

#include "cachelab.h"               // header file for cachelab.c
#include <stdio.h>                  // stdio.h used for input/output functions
#include <stdlib.h>                 // stdlib.h used for malloc()
#include <unistd.h>                 // unistd.h used for getopt()
#include <math.h>                   // math.h used for pow()
#include <stdbool.h>                // stdbool.h used for bool data type
#include <string.h>                 // string.h used for memcpy()

// Cachelab Constants
    // Algorithm types
const char *LRU = "lru\0";          // Constant string comparison for lru algorithm
const char *FIFO = "fifo";          // Constant string comparison for fifo algorithm
const char *OPTIMAL = "optimal";    // Constant string comparison for optimal algorithm
    // Integer constants
const int NUMARGS = 13;             // Number of given arguments for error check
const int HEXMAX = 128;             // Maximum characters of a given hexidecimal address 

// Cachelab Variables
    // Args
int addressSize;                    // (m) The size of address used in the cache (bit)
int setBits;                        // (s) The number of set bits (S = 2^s)
int linesPerSet;                    // (e) Number of lines per set
int blockOffsetBits;                // (b) Number of set index bits
char *fileName;                     // (i) Name of file containing addresses
char *algorithm;                    // (r) Page Replacement Algorithm - LRU/FIFO/Optimal
    // Function variables
int opt;                            // Option for switch case to gather argument list
int numSets;                        // (S) Number of sets
int numLines;                       // (E) Number of lines; associativity
int blockSize;                      // (B) The block size in bits
int tagSize;                        // The tag size in bits
int size;                           // Number of memory blocks within file for calculation of result
int i;                              // Index counter for moving through arrays
int j;                              // Index counter for moving through arrays
    // Cache counters
int hits;                           // Hit counter
int misses;                         // Miss counter
int evictions;                      // Evictions counter
    // Conversion variables
char *hexAddress;                   // Char array containing single address in hexidecimal
char *binaryAddress;                // Char array containing single address in binary
    // Result calculations
double missRate;                    // Miss rate casted to integer for printing result
int avgAccessTime;                  // Average access time calculated for printing result
int runTime;                        // Run time calculated for printing result
    // Input file
FILE *pFile;                        // Input file pointer

// CacheLine Struct
typedef struct{
    bool validBit;              // Valid Bit showing use of CacheLine Block: True/1 = in use; False/0 = not in use.
    unsigned long long tag;     // Tag Line segment of CacheLine Block
    int lruCount;               // Counter for uses for LRU algorithm
} CacheLine;

// Default empty CacheLine
CacheLine emptyLine = {.validBit = false, .tag = 0, .lruCount = 0};

// Cache
CacheLine **cache;                  // Struct CacheLine double pointer = cache

// ****************************************************************************************************
// Main Function
//
// ****************************************************************************************************
int main(int argc, char **argv)
{
    // Close program if argument list contains wrong number of arguments
    if(argc != NUMARGS)
    {
        printf("[ERROR] Invalid number of arguments given ... [EXITING PROGRAM]\n");
        return 0;
    }

    // Initialize argument list into empty cache
    initialize(argc, argv);

    // Allocate cache
    cache = (CacheLine **)malloc(sizeof(CacheLine *) * numSets);
    for (i = 0; i < numSets; i++) 
    {
        cache[i] = (CacheLine *)malloc(sizeof(CacheLine) * numLines);
        for (j = 0; j < numLines; j++)
        {
            //Set cache[i][j] to emptyLine
            cache[i][j] = emptyLine;
            //printf("Cache[%i][%i] : ValidBit: %i Tag: %lli LRUCount: %i\n", i, j, cache[i][j].validBit, cache[i][j].tag, cache[i][j].lruCount); // ---------- Cache Allocation Check
        }
    }

    // Open file containing addresses
    pFile = fopen(fileName, "r");

    // Exit if file is empty
    if(pFile == NULL)
    {
        printf("[ERROR] File = NULL ... [EXITING PROGRAM]\n");
        return 0;
    }

    // Initialize hits, misses, and evictions to 0
    hits = 0;
    misses = 0;
    evictions = 0;

    // Allocate memory for hexAddress
    hexAddress = malloc(HEXMAX * sizeof(char));

    // While addresses left in input file, read each line and store as string hexAddress
    while(fscanf(pFile, "%s", hexAddress) > 0)
    {
        // Print line of file
        printf("%s ", hexAddress);

        // Convert hexidecimal line to binary
        binaryAddress = hexToBinary(hexAddress);
        //printf("\nBinary Address: %s\n", binaryAddress); // ----------------------------------------------------------------------------------------- Binary address check
        //printf("Length of Binary Address: %li\n", strlen(binaryAddress)); // ------------------------------------------------------------------------ Length of binary address check

        // Compare to cache display result
        cacheSim(binaryAddress);
        
        // Increment size (number of addresses within file) for result calculations
        size++;
    }

    // Calculate miss rate as decimal percentage
    missRate = ((misses * 100) / (hits + misses)); // ------------------------------------------------------------------- This double auto rounds to lower whole digit?

    // Calculate average access time using missRateFloat
    avgAccessTime = averageAccessTime(missRate);

    // Cast missRateFloat into integer missRate to display
    //missRate = (int)(missRate); // ------------------------------------------------------------------------------------ Not needed considering the comment above

    // Calculate total run time
    runTime = totalRunTime(size, avgAccessTime);

    // Print result
    printResult(hits, misses, missRate, runTime);

    // Remember to close file when done
    fclose(pFile);

    // Free space from malloc pointers
    for (i = 0; i < numSets - 1; i++) // ---------------------------------------------------------------- RESULTS IN SEGMENTATION FAULT WHEN FREEING LAST ARRAY INDEX???
    {
        free(cache[i]);
        //printf("TEST CHECKPOINT - FREE MALLOC'D CACHE[%i] SUCCESS\n", i); // ------------------------------------------------------------- Free memory of each cache set test
    }
    free(cache);
    free(binaryAddress);
    free(hexAddress);
}

// ****************************************************************************************************
// Initialize Function
// --- Initializes the list of given arguments in the form :m:s:e:b:i:r: into the
// --- variables listed below and then calculates S(numSets), E(numLines), B(blockSize), 
// --- and tagSize before allocating the cache with malloc and setting each block to the
// --- default emptyBlock
// ---                      m = addressSize
// ---                      s = setBits
// ---                      e = linesPerSet
// ---                      b = blockOffsetBits
// ---                      i = fileName
// ---                      r = algorithm
// ****************************************************************************************************
void initialize(int argc, char **argv)
{
    // Function Variables
    int opt;             // Option for switch case to gather argument list

    // Initialize Argument List Using getopt() Function
    while ((opt = getopt(argc, argv, ":m:s:e:b:i:r:")) != -1)
    {
        switch (opt)
        {
            case 'm':
                addressSize = atoi(optarg);
                //printf("addressSize: %i\n", addressSize);
                break;
            case 's':
                setBits = atoi(optarg);
                //printf("setBits: %i\n", setBits);
                break;
            case 'e':
                linesPerSet = atoi(optarg);
                //printf("linesPerSet: %i\n", linesPerSet);
                break;
            case 'b':
                blockOffsetBits = atoi(optarg);
                //printf("blockOffsetBits: %i\n", blockOffsetBits);
                break;
            case 'i':
                fileName = optarg;
                //printf("fileName: %s\n", fileName);
                break;
            case 'r':
                algorithm = optarg;
                //printf("algorithm: %s\n", algorithm);
                break;
            default:
                printf("Error: Please check format of arguments ... \n");
        }
    }

    // Calculate Number of Sets (S = 2^s)
    numSets = pow(2, setBits);

    // Calculate Number of Lines (E = 2^e)
    numLines = pow(2, linesPerSet);

    // Calculate Block Size (B = 2^b)
    blockSize = pow(2, blockOffsetBits);

    // Calculate Tag Size
    tagSize = ( addressSize - setBits ) - blockOffsetBits;

}

// ****************************************************************************************************
// Cache Sim Function
// --- Simulates the cache by comparing the given cache struct and char* address, a string
// --- representation of the address in binary being searched in the cache
// ****************************************************************************************************
void cacheSim(char *address)
{
    // CacheSim Variables
    char *tagString;                // String representation of tag from address parameter
    char *setBuffer;                // String representation of set and offset
    char *setString;                // String representation of tag from address parameter
    unsigned long long tag;         // Integer representation of tag from address parameter
    unsigned long long set;         // Integer representation of set from address parameter
    int lruIndex;                   // Index of least recently used within cache set
    bool usedFlag = false;          // Flag indicating insertion of searchAddress into cache / hit within cache
    bool fullSet = false;           // Flag indicating a full set within the cache

    // Extract tag and set from address as strings and convert to integers
    tag = binToDecimal(atoi(memcpy(tagString, address, tagSize)));
    setBuffer = &address[tagSize];
    strncpy(setString, setBuffer, setBits);
    set = binToDecimal(atoi(setString));

    //printf("Tag String: %s\n", tagString); // ------------------------------------------------------------------------------------------------------ Tag string check
    //printf("Tag: %llu\n", tag); // ----------------------------------------------------------------------------------------------------------------- Tag check
    //printf("Length of Tag String: %li\n", strlen(tagString)); // ----------------------------------------------------------------------------------- Length of tag string check
    //printf("&address[tagSize]: %s\n", &address[tagSize]); // --------------------------------------------------------------------------------------- &address[tagSize] check
    //printf("Set String: %s\n", setString); // ------------------------------------------------------------------------------------------------------ Set string check
    //printf("Set: %llu\n", set); // ----------------------------------------------------------------------------------------------------------------- Set check

    // If algorithm is LRU, perform least recently used algorithm
    if(strcmp(algorithm, LRU) == 0)
    {
        // For each cache line within the set
        for (i = 0; i < numLines; i++)
        {
            // If memory hasn't been added into cache
            if(usedFlag == false)
            {
                // If cache line is empty, insert memory and increment lruCount and misses
                if (cache[set][i].validBit == false)
                {
                    cache[set][i].validBit = true;
                    cache[set][i].tag = tag;
                    cache[set][i].lruCount++;
                    misses++;
                    printf("M\n");
                    usedFlag = true;
                    //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli LRUCount: %i\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].lruCount); // ------- Cache block check
                }
                // Else if cache line tag equals searchAddress tag, display hit and increment lruCount and hits
                else if (cache[set][i].tag == tag)
                {
                    cache[set][i].lruCount++;
                    hits++;
                    printf("H\n");
                    usedFlag = true;
                    //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli LRUCount: %i\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].lruCount); // ------- Cache block check
                }
                // Else cache line is full, set fullSet flag to true
                else
                {
                    fullSet = true;
                }
            }   
        }
        // If address wasn't hit and cache is full, evict LRU and replace
        if(usedFlag == false && fullSet == true)
        {
            lruIndex = leastRecentlyUsed(set);
            cache[set][lruIndex].validBit = true;
            cache[set][lruIndex].tag = tag;
            cache[set][lruIndex].lruCount++;
            misses++;
            evictions++;
            printf("M\n"); 
            //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli LRUCount: %i\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].lruCount); // --------------- Cache block check
        } 
    }
    // Else if algorithm is FIFO then perform first-in first-out algorithm
    else if(strcmp(algorithm, FIFO) == 0)
    {
        printf("\n[ERROR] First-In First-Out Algorithm Not Available ...\n");
    }
    // Else if algorithm is OPTIMAL then perform optimal algorithm
    else if(strcmp(algorithm, OPTIMAL) == 0)
    {
        printf("\n[ERROR] Optimal Algorithm Not Available ...\n");
    }
    // Else algorithm is not valid
    else
    {
        printf("\n[ERROR] Given Algorithm Invalid - Use lru, fifo, or optimal ...\n");
    }
}

// ****************************************************************************************************
// Least Recently Used Function
// --- Sets least to the lruCount of the first line in the set and then compares each line of the set
// --- returning the index of the least recently used block within the given set
// ****************************************************************************************************
int leastRecentlyUsed(int set)
{
    int least = cache[set][0].lruCount;
    int index = 0;

    for (i = 0; i < numLines; i++)
    {
        if (least >= cache[set][i].lruCount)
        {
            least = cache[set][i].lruCount;
            index = i;
        }
    }

    return index;
}

// ****************************************************************************************************
// Hexidecimal To Binary Function
// --- Receives char array in the form of a hexidecimal representation and converts it to binary
// --- and returns the resulting binary char array
// ****************************************************************************************************
char *hexToBinary(char *hex)
{
    // Function Variables
    int addZeros;                   // Used for adding sets of "0000" to the binary address before the hexidecimal conversion

    // Allocate memory for binary character array
    binaryAddress = malloc( addressSize * sizeof(char) );

    // Add zeros to make address proper length of bits
    for(addZeros = ((addressSize - (strlen(hex) * 4)) / 4); addZeros > 0; addZeros--)
    {
        strncat(binaryAddress, "0000", 5);
    }
    // For each char of hex add binary conversion to new binary string
    for (i = 0; i < strlen(hex); i++)
    {
        if(hex[i] == '0')
            strncat(binaryAddress, "0000", 5);
        else if(hex[i] == '1')
            strncat(binaryAddress, "0001", 5);
        else if(hex[i] == '2')
            strncat(binaryAddress, "0010", 5);
        else if(hex[i] == '3')
            strncat(binaryAddress, "0011", 5);
        else if(hex[i] == '4')
            strncat(binaryAddress, "0100", 5);
        else if(hex[i] == '5')
            strncat(binaryAddress, "0101", 5);
        else if(hex[i] == '6')
            strncat(binaryAddress, "0110", 5);
        else if(hex[i] == '7')
            strncat(binaryAddress, "0111", 5);
        else if(hex[i] == '8')
            strncat(binaryAddress, "1000", 5);
        else if(hex[i] == '9')
            strncat(binaryAddress, "1001", 5);
        else if(hex[i] == 'A')
            strncat(binaryAddress, "1010", 5);
        else if(hex[i] == 'B')
            strncat(binaryAddress, "1011", 5);
        else if(hex[i] == 'C')
            strncat(binaryAddress, "1100", 5);
        else if(hex[i] == 'D')
            strncat(binaryAddress, "1101", 5);
        else if(hex[i] == 'E')
            strncat(binaryAddress, "1110", 5);
        else if(hex[i] == 'F')
            strncat(binaryAddress, "1111", 5);
    }

    // Return binaryAddress char*
    return binaryAddress;
}

// ****************************************************************************************************
// Binary To Decimal Function
// --- Converts binary to decimal
// ****************************************************************************************************
int binToDecimal(unsigned long long binary)
{
  int decimal = 0;                  // Decimal conversion of binary
  int i = 0;                        // Index for conversion
  int remainder;                    // Remainder of modulus 10 for conversion to decimal

  while (binary != 0) 
  {
    remainder = binary % 10;
    binary /= 10;
    decimal += remainder * pow(2, i);
    ++i;
  }

  return decimal;
}

// ****************************************************************************************************
// Average Access Time Function
// --- Calculates avgAccessTime for calculation of runTime for printResult function
// ****************************************************************************************************
int averageAccessTime(double missRate)
{
    int avgAccessTime = HIT_TIME + ( missRate );
    //printf("Average Access Time: %i\n", avgAccessTime); // --------------------------------------------------------------------- Average access time check

    return avgAccessTime;
}

// ****************************************************************************************************
// Total Run Time Function
// --- Calculates runTime for printResult function
// ****************************************************************************************************
int totalRunTime(int size, int avgAccessTime)
{
    int runTime = size * avgAccessTime;

    return runTime;
}

// ****************************************************************************************************
// Print Result Function
//
// ****************************************************************************************************
void printResult(int hits, int misses, int missRate, int runTime)
{
    printf("[result] hits: %d misses %d miss rate: %d%% total running time: %d cycle\n", hits, misses, missRate, runTime);
}