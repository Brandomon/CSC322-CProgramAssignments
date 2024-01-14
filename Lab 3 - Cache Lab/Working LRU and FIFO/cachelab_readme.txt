cachelab_readme.txt

Name: Brandon LaPointe
ID: 804239455
Account: blapoint@cs.oswego.edu

Extra work:
FIFO has been verified to work with the given test argument. I attempted Optimal but could not seem to get
by the segmentation faults coming from the additional code so it is all commented out. All of my attempt
at the optimal algorithm is still within the code if it would be possible to get partial extra credit for
the try, I would be much appreciative.

Special Instructions for compiling the program:
Must link math library using -lm at end of compiling command line.
ex.) gcc cachelab-blapoint.c -o cachelab -lm

Special Instructions for running the program:
Using the school servers, some of the tests would run into a segmentation fault part way through the code,
usually after the third address result, and would eventually run properly if ran several times.
Sometimes they will work the first time, sometimes it takes anywhere from 2 - 7 attempts of
running the same argument list through the servers to get the program to display the
entire results. This took me several days to discover and a lot of time was wasted
going back and forth with my code attempting to figure out why the changes I made
didn't correspond to any changes in the behavior of the program but eventually 
I made the connection but still don't understand exactly why this is happening.
