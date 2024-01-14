//
//  cachelab.h - header file for cachelab
//  : defining global variabls and functions
//

#ifndef cachelab_h
#define cachelab_h

#define HIT_TIME 1          // hit time fixed for calculating running time
#define MISS_PENALTY 100    // miss penalty fixed for calculating running time

void printResult(int hits, int misses, int missRate, int runTime);
void initialize(int argc, char *argv[]);
int averageAccessTime(double missRate);
int totalRunTime(int numCode, int avgAccessTime);
void generateCache(int numSets, int numLines);
void cacheSim(char *address);
int leastRecentlyUsed(int set);
char *hexToBinary(char *hex);
int binToDecimal(unsigned long long binary);

#endif /* cachelab_h */
