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
long double averageAccessTime(long double missRate);
int totalRunTime(int numCode, long double avgAccessTime);
void generateCache(int numSets, int numLines);
void cacheSim(char *address);
int leastRecentlyUsed(int set);
int firstInFirstOut(int set);
/*int optimal(int set);
int farthestInFuture(unsigned long long tag, unsigned long long set);*/
char *hexToBinary(char *hex);
int binToDecimal(unsigned long long binary);

#endif /* cachelab_h */
