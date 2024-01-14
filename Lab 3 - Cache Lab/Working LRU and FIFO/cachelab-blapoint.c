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

// // // Cachelab Constants
// Algorithm types
const char *LRU = "lru";            // Constant string comparison for lru algorithm
const char *FIFO = "fifo";          // Constant string comparison for fifo algorithm
const char *OPTIMAL = "optimal";    // Constant string comparison for optimal algorithm

// Integer constants
const int NUMARGS = 13;             // Number of given arguments for error check
const int MAX3HEX = 4095;           // Maximum decimal number of 3 digit hexidecimal number 0xFFF
const int MAX4HEX = 65535;          // Maximum decimal number of 4 digit hexidecimal number 0xFFFF

// // // Cachelab Variables
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

// Index counters
int i;                              // Index counter for moving through arrays
int j;                              // Index counter for moving through arrays

// Cache counters
int hits;                           // Hit counter
int misses;                         // Miss counter
int evictions;                      // Evictions counter
int clock;                          // "Clock" time counter for algorithm implementation

// Conversion variables
char *hexAddress;                   // Char array containing single address in hexidecimal
char *binaryAddress;                // Char array containing single address in binary

// Result calculations
long double missRate;               // Miss rate casted to integer for printing result
long double avgAccessTime;          // Average access time calculated for printing result
int runTime;                        // Run time calculated for printing result

// Input file pointer
FILE *pFile;                        // Input file pointer

// CacheLine Struct
typedef struct{
    bool validBit;                  // Valid Bit showing use of CacheLine Block: True/1 = in use; False/0 = not in use.
    unsigned long long tag;         // Tag Line segment of CacheLine Block
    int lruCount;                   // Counter for uses for LRU algorithm
    int fifoCount;                  // Counter for uses for FIFO algorithm
/*  int futureCount;                // Counter for number of memory addresses until next occurrence */
} CacheLine;

// Default empty CacheLine          // Sets all CacheLine struct variables to defaults
CacheLine emptyLine = {.validBit = false, .tag = 0, .lruCount = 0, .fifoCount = 0/*, .futureCount = 0*/};

// Cache double pointer
CacheLine **cache;                  // Struct CacheLine double pointer = cache

// ****************************************************************************************************
// Main Function
// --- Given a list of arguments in the form :m:s:e:b:i:r: the program will simulate a cache using the
// --- input arguments and a file with a list of addresses in hexidecimal addresses, displaying the
// --- hexidecimal address followed by 'H' for hit or 'M' for miss. The program will then calculate
// --- the results of the simulated cache and display the results using the given printResult function.
// ---
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
            //printf("Cache[%i][%i] : ValidBit: %i Tag: %lli LRUCount: %i\n", i, j, cache[i][j].validBit, cache[i][j].tag, cache[i][j].lruCount); // ----------- Cache Allocation Check
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
    hexAddress = malloc(50 * sizeof(char));

    // Initialize "clock" time
    clock = 0;

    // If algorithm is optimal
    if(strcmp(algorithm, OPTIMAL) == 0)
    {
        // Print error for optimal algorithm not being complete
        printf("[ERROR] Optimal Algorithm Not Available ...\n");
    }

    // While addresses left in input file, read each line and store as string hexAddress
    while(fscanf(pFile, "%s", hexAddress) > 0)
    {
        // Print line of file
        printf("%s ", hexAddress);

        // Convert hexidecimal line to binary
        binaryAddress = hexToBinary(hexAddress);
        //printf("\nBinary Address: %s\n", binaryAddress); // ---------------------------------------------------------------------------------------------------- Binary address check
        //printf("Length of Binary Address: %li\n", strlen(binaryAddress)); // ------------------------------------------------------------------------- Length of binary address check

        // Compare to cache display result
        cacheSim(binaryAddress);
        
        // Increment size (number of addresses within file) for result calculations
        size++;
    }

    // Free malloc'd hexAddress and binaryAddress memory
    free(hexAddress);
    free(binaryAddress);

    // Calculate miss rate as decimal percentage casting misses and hits to float
    missRate = (((float)(misses) * 100) / ((float)(hits) + misses));
    //printf("Miss Rate: %Lf\n", missRate); // ------------------------------------------------------------------------------------------------------------------------ Miss rate check
    
    // Calculate average access time using missRateFloat
    avgAccessTime = averageAccessTime(missRate);

    // Calculate total run time
    runTime = totalRunTime(size, avgAccessTime);

    // Print result
    printResult(hits, misses, missRate, runTime);

    // Remember to close file when done
    fclose(pFile);

    // Free malloc'd cache memory
    for (i = 0; i < numSets - 1; i++) // ------------------------------------- (i = 0; i < numSets; i++) -> attempt to free the last cache[i] causes a segmentation fault (core dumped)
    {
        free(cache[i]);
        //printf("TEST CHECKPOINT - FREE MALLOC'D CACHE[%i] SUCCESS\n", i); // -------------------------------------------------------------------- Free memory of each cache set check
    }
    free(cache);
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
                exit(1);
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
    int fifoIndex;                  // Index of first in within the cache set
    int futureIndex;                // Index of the cache line used furthest in the future
    bool insertFlag = false;        // Flag indicating insertion of searchAddress into cache
    bool hitFlag = false;           // Flag indicating hit of searchAddress within the cache
    bool fullSet = false;           // Flag indicating a full set within the cache

    // Extract tag from address as string and convert to unsigned long long integer
    tag = binToDecimal((unsigned long long)atoi(memcpy(tagString, address, tagSize)));
    setBuffer = &address[tagSize];

    // Extract set from address as string and convert to unsigned long long integer
    strncpy(setString, setBuffer, setBits);
    set = binToDecimal(atoi(setString));

    // TEST PRINTOUTS ( Uncomment to display within output)
    //printf("\nTag String: %s\n", tagString); // -------------------------------------------------------------------------------------------------------------------- Tag string check
    //printf("Tag: %llu\n", tag); // ---------------------------------------------------------------------------------------------------------------------------------------- Tag check
    //printf("Length of Tag String: %li\n", strlen(tagString)); // ----------------------------------------------------------------------------------------- Length of tag string check
    //printf("&address[tagSize] - Set and offset bits: %s\n", &address[tagSize]); // -------------------------------------------------------------------------- &address[tagSize] check
    //printf("Set String: %s\n", setString); // ---------------------------------------------------------------------------------------------------------------------- Set string check
    //printf("Set: %llu\n", set); // ---------------------------------------------------------------------------------------------------------------------------------------- Set check

    // If algorithm is LRU, perform least recently used algorithm
    if(strcmp(algorithm, LRU) == 0)
    {
        // For each cache line within the set
        for (i = 0; i < numLines; i++)
        {
            // Increment "clock" time for each line
            clock++;

            // If memory hasn't been added into cache
            if(insertFlag == false && hitFlag == false)
            {
                // If cache line is empty, insert memory and increment lruCount and misses
                if (cache[set][i].validBit == false)
                {
                    cache[set][i].validBit = true;
                    cache[set][i].tag = tag;
                    cache[set][i].lruCount = clock;
                    misses++;
                    printf("M\n");
                    insertFlag = true;
                    //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli LRUCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].lruCount); // ------------ Cache block check
                }
                // Else if cache line tag equals searchAddress tag, display hit and increment lruCount and hits
                else if (cache[set][i].validBit == true && cache[set][i].tag == tag)
                {
                    cache[set][i].lruCount = clock;
                    hits++;
                    printf("H\n");
                    hitFlag = true;
                    //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli LRUCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].lruCount); // ------------ Cache block check
                }
                // Else cache set is full, set fullSet flag to true
                else //if (i == numLines - 1 && cache[set][i].validBit == true)
                {
                    fullSet = true;
                }
            }   
        }
        // If address wasn't hit and cache is full, evict LRU and replace
        if(insertFlag == false && hitFlag == false && fullSet == true)
        {
            lruIndex = leastRecentlyUsed(set);
            cache[set][lruIndex].validBit = true;
            cache[set][lruIndex].tag = tag;
            cache[set][lruIndex].lruCount = clock;
            misses++;
            evictions++;
            printf("M\n"); 
            //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli LRUCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].lruCount); // -------------------- Cache block check
        } 
    }
    // Else if algorithm is FIFO then perform first-in first-out algorithm
    else if(strcmp(algorithm, FIFO) == 0)
    {
        // For each cache line within the set
        for (i = 0; i < numLines; i++)
        {
            // If memory hasn't been added into cache or found within the cache
            if(insertFlag == false && hitFlag == false)
            {
                // If cache line is empty, insert memory and increment fifoCount and misses
                if (cache[set][i].validBit == false)
                {
                    cache[set][i].validBit = true;
                    cache[set][i].tag = tag;
                    cache[set][i].fifoCount = 0;
                    misses++;
                    printf("M\n");
                    insertFlag = true;
                    //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli fifoCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].fifoCount); // ---------- Cache block check
                }
                // Else if cache line tag equals searchAddress tag, display hit and increment lruCount and hits
                else if (cache[set][i].validBit == true && cache[set][i].tag == tag)
                {
                    cache[set][i].validBit = true;
                    cache[set][i].tag = tag;
                    hits++;
                    printf("H\n");
                    insertFlag = true;
                    //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli fifoCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].fifoCount); // ---------- Cache block check
                }
                // Else cache line is full, increment fifoCount of current cacheline
                else if (cache[set][i].validBit == true)
                {
                    cache[set][i].fifoCount++;
                }
                // Else cache set is full, set fullSet flag to true
                if (cache[set][numLines - 1].validBit == true)
                {
                    fullSet = true;
                }
            }   
        }
        // If address wasn't hit and cache is full, evict First In and replace
        if(insertFlag == false && hitFlag == false && fullSet == true)
        {
            fifoIndex = firstInFirstOut(set);
            cache[set][fifoIndex].validBit = true;
            cache[set][fifoIndex].tag = tag;
            cache[set][fifoIndex].fifoCount = 0;
            misses++;
            evictions++;
            printf("M\n"); 
            //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli fifoCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].fifoCount); // ------------------ Cache block check
        } 
    }
    // Else if algorithm is OPTIMAL then perform optimal algorithm
    else if(strcmp(algorithm, OPTIMAL) == 0)
    {
        printf("\n");

        /**************************************************************************************************************************************************************************************************************
         
        // For each cache line within the set
        for (i = 0; i < numLines; i++)
        {
            // If memory hasn't been added into cache or found within the cache
            if(insertFlag == false && hitFlag == false)
            {
                // If cache line is empty, insert memory, increment misses, and set futureCount using farthestInFuture function
                if (cache[set][i].validBit == false)
                {
                    cache[set][i].validBit = true;
                    cache[set][i].tag = tag;
                    cache[set][i].futureCount = farthestInFuture(tag, set);
                    misses++;
                    printf("M\n");
                    insertFlag = true;
                    //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli futureCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].futureCount); // ----------------------- Cache block check
                }
                // Else if cache line tag equals searchAddress tag, display hit, increment hits, and get futureCount
                else if (cache[set][i].validBit == true && cache[set][i].tag == tag)
                {
                    cache[set][i].validBit = true;
                    cache[set][i].tag = tag;
                    cache[set][i].futureCount = farthestInFuture(tag, set);
                    hits++;
                    printf("H\n");
                    insertFlag = true;
                    //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli futureCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].futureCount); // ----------------------- Cache block check
                }
                // Else cache line is full, decrement futureCount of current cacheline
                else if (cache[set][i].validBit == true)
                {
                    cache[set][i].futureCount--;
                }
                // Else cache set is full, set fullSet flag to true
                if (cache[set][numLines - 1].validBit == true)
                {
                    fullSet = true;
                }
            }   
        }
        // If address wasn't hit and cache is full, evict farthestInFuture and replace
        if(insertFlag == false && hitFlag == false && fullSet == true)
        {
            futureIndex = optimal(set);
            cache[set][futureIndex].validBit = true;
            cache[set][futureIndex].tag = tag;
            cache[set][futureIndex].futureCount = farthestInFuture(cache[set][futureIndex].tag, set);
            misses++;
            evictions++;
            printf("M\n"); 
            //printf("Cache[%lli][%i] : ValidBit: %i Tag: %lli futureCount: %i\n\n", set, i, cache[set][i].validBit, cache[set][i].tag, cache[set][i].futureCount); // ------------------------------- Cache block check
        } 
        
        **************************************************************************************************************************************************************************************************************/
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
// --- returning the index of the least recently used block within the given set.
// --- Lower number = least recent.
// ****************************************************************************************************
int leastRecentlyUsed(int set)
{
    // Function Variables
    int least = cache[set][0].lruCount;
    int index = 0;

    // For each line of cache of given set
    for (i = 0; i < numLines; i++)
    {
        // If least is less than cache[set][i].lruCount
        if (least >= cache[set][i].lruCount)
        {
            // Set least to cache[set][i].lruCount and mark index
            least = cache[set][i].lruCount;
            index = i;
        }
    }

    return index;
}

// ****************************************************************************************************
// First In First Out Function
// --- Sets firstIn to the fifoCount of the first line in the set and then compares each line of the set
// --- returning the index of the first in block within the given set.
// --- Higher number = first in.
// ****************************************************************************************************
int firstInFirstOut(int set)
{
    // Function Variables
    int firstIn = cache[set][0].fifoCount;
    int index = 0;

    // For each line of cache of given set
    for (i = 0; i < numLines; i++)
    {
        // If firstIn is less than cache[set][i].fifoCount
        if (firstIn < cache[set][i].fifoCount)
        {
            // Set firstIn to cache[set][i].fifoCount and mark index
            firstIn = cache[set][i].fifoCount;
            index = i;
        }
    }

    return index;
}

/***********************************************************************************************************************************************************************************************************************
//
// ****************************************************************************************************
// Optimal Function
// --- Sets farthestInFuture to the futureCount of the first line in the set and then compares each line
// --- of the set, returning the index of the farthest block within the given set.
// --- Higher number = farthestInFuture.
// ****************************************************************************************************
int optimal(int set)
{
    // Function Variables
    int farInFuture = cache[set][0].futureCount;
    int index = 0;

    // For each line of cache of given set
    for (i = 0; i < numLines; i++)
    {
        // If farthestInFuture is less than cache[set][i].futureCount
        if (farInFuture < cache[set][i].futureCount)
        {
            // Set farthestInFuture to cache[set][i].futureCount and mark index
            farInFuture = cache[set][i].futureCount;
            index = i;
        }
    }

    return index;
}

// ****************************************************************************************************
// Farthest In Future Function
// ****************************************************************************************************
// --- Pseudocode:
// --- Create a list of future queries from the input file
// --- Set futureCount to nearest second occurance of memory in list
// ---      Search for address in future queries list
// ---      Mark first occurrence of address item index
// ---      Set futureIndex equal to index of next same item in future queries list minus the first occurrence index
// --- Return futureIndex
// ---
// --- Pseudocode v2:
// --- Initialize firstFlag and secondFlag to false
// --- Allocate memory for new hex address and binary address for conversion
// --- For each scanned address of input file
// ---      Convert to binary
// ---      Extract the tag and set
// ---      Convert tag and set to decimal
// ---      Compare tags and sets
// ---          Mark first occurrence of given tag and set
// ---          Mark second occurrence of given tag and set
// ---          futureIndex = secondIndex - firstIndex
// --- Return futureIndex
//
// --- Possible error: Will only give the distance between the first two occurrences
// ---                  If there are more than 2 occurrences, the result will only
// ---                  come from the first two.
// ****************************************************************************************************
int farthestInFuture(unsigned long long tag, unsigned long long set)
{
    // Function Variables
    char *compHexAddress;           // Char array containing single address in hexidecimal for future queries list
    char *compBinaryAddress;        // Char array containing single address in binary for future queries list
    char *compTagString;            // String representation of tag from future queries
    char *compSetBuffer;            // String representation of set and offset of future queries
    char *compSetString;            // String representation of tag from future queries for comparison
    unsigned long long compTag;     // Integer representation of tag from future queries
    unsigned long long compSet;     // Integer representation of set from future queries
    bool firstFlag = false;         // Boolean flag representing that the firstIndex was set
    bool secondFlag = false;        // Boolean flag representing that the futureIndex was set
    int firstIndex;                 // Integer representing the number of indexes until the first occurrence of the given set and tag
    int futureIndex;                // Integer representing the number of indexes until the next occurrence of the given set and tag
    int index;                      // Index for marking futureIndex

    // Allocate memory for compHexAddress
    compHexAddress = malloc(addressSize * sizeof(char));

    // Allocate memory for compBinaryAddress
    compBinaryAddress = malloc(MAX3HEX * sizeof(char));

    // While addresses left in input file, store each within a list of future queries
    while(fscanf(pFile, "%s", compHexAddress) > 0)
    {
        if(usedFlag == false)
        {
            // Convert hexidecimal line to binary
            compBinaryAddress = hexToBinary(compHexAddress);

            // Extract tag from address as string and convert to unsigned long long integer
            compTag = binToDecimal((unsigned long long)atoi(memcpy(compTagString, compBinaryAddress, tagSize)));
            compSetBuffer = &compBinaryAddress[tagSize];

            // Extract set from address as string and convert to unsigned long long integer
            strncpy(compSetString, compSetBuffer, setBits);
            compSet = binToDecimal(atoi(compSetString));

            // If the given tag and set match the comparison tag and set and the firstFlag and secondFlag are false
            if(tag == compTag && set == compSet && firstFlag == false && secondFlag == false)
            {
                // Mark the index of the first occurrence of the set and tag
                firstIndex = index;
                firstFlag = true;
            }
            
            // If the given tag and set match the comparison tag and set and the firstFlag is true and the secondFlag is false
            if(tag == compTag && set == compSet && firstFlag == true && secondFlag == false)
            {
                // Mark the index of the second occurrence of the set and tag
                futureIndex = index - firstIndex;
                secondFlag = true;
            }
        }
        
        index++;
    }

    // Free malloc'd memory from compHexAddress and compBinaryAddress
    free(compHexAddress);
    free(compBinaryAddress);

    return futureIndex;
}

************************************************************************************************************************************************************************************************************************/

// ****************************************************************************************************
// Hexidecimal To Binary Function
// --- Receives char array in the form of a hexidecimal representation and converts it to binary
// --- and returns the resulting binary char array
// ****************************************************************************************************
char *hexToBinary(char *hex)
{
    // Function Variables
    int addZeros;                   // Used for adding sets of '0' to the binary address before the hexidecimal conversion

    // Allocate memory for binary character array
    binaryAddress = malloc(MAX3HEX * sizeof(char));

    // Add zeros to make address proper length of bits
    for(addZeros = (addressSize - (strlen(hex) * 4)); addZeros > 0; addZeros--)
    {
        strncat(binaryAddress, "0", 2);
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
long double averageAccessTime(long double missRate)
{
    long double avgAccessTime = (float)(HIT_TIME) + ( missRate / 100 * (float)(MISS_PENALTY));
    //printf("Average Access Time: %Lf\n", avgAccessTime); // --------------------------------------------------------------------------------------------------- Average access time check

    return avgAccessTime;
}

// ****************************************************************************************************
// Total Run Time Function
// --- Calculates runTime for printResult function
// ****************************************************************************************************
int totalRunTime(int size, long double avgAccessTime)
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