/**********************************************************************************************************************
 * blapoint-tsh.c - A tiny shell program with job control
 * 
 * Name: Brandon LaPointe 
 * StudentID: blapoint@cs.oswego.edu
 * StudentID#: 804239455
 * 
 **********************************************************************************************************************
 * 
 *                                                      Notes:
 * 
 **********************************************************************************************************************
 *                                      Remaining Empty Functions to Complete:
 * 
 *      eval: Main routine that parses and interprets the command line. [~70 lines]
 *      builtin cmd: Recognizes and interprets the built-in commands: quit, fg, bg, and jobs. [~25 lines]
 *      do_bgfg: Implements the bg and fg built-in commands. [~50 lines]
 *      waitfg: Waits for a foreground job to complete. [~20 lines]
 *      sigchld handler: Catches SIGCHILD signals. [~80 lines]
 *      sigint handler: Catches SIGINT (ctrl-c) signals. [~15 lines]
 *      sigtstp handler: Catches SIGTSTP (ctrl-z) signals. [~15 lines]
 * 
 **********************************************************************************************************************
 * 
 *                                                      Shell:
 *      A shell is an interactive command-line interpreter that runs programs on behalf of the user.
 *      - The shell repeatedly prints a prompt, waits for a command line on stdin, and then carries out some action
 *        as directed by the contents of the command line.
 * 
 *                                                   Command Line:
 *      The command line is a sequence of ASCII texts words delimited by whitespace.
 *      - The first word is either the name of a built-in command or the pathname of an executable file
 *      - The remaining words are command-line arguments
 * 
 *      - If the first word is a built-in command, the shell immediately executes the command in the current process.
 *      - Otherwise,  the word is assumed to be the pathname of an executable program. In this case, the shell forks
 *        a child process, then loads and runs the program in the context of the child. The child processes created
 *        as a result of interpreting a single command line are known collectively as a job. In general, a job can
 *        consist of multiple child processes connected by Unix pipes.
 * 
 *      If the command line ends with '&', the job runs in the background
 *                      Background
 *                      • Shell does NOT wait for the job to terminate before printing the prompt, awaiting the next command line
 *                      • An arbitrary number of jobs can run in the background
 * 
 *      Otherwise the command line job runs in the foreground
 *                      Foreground
 *                      • Shell waits for the job to terminate before printing the prompt, awaiting the next command line
 *                      • Only 1 job can be running in the foreground
 *      
 **********************************************************************************************************************
 *      Unix shells also provide various built-in commands that support job control. For example:
 *          • jobs      : List the running and stopped background jobs.
 *          • bg <job>  : Change a stopped background job to a running background job.
 *          • fg <job>  : Change a stopped or running background job to a running in the foreground.
 *          • kill <job>: Terminate a job.
 * 
 **********************************************************************************************************************
 *                                                      Hints:
 * 
 * • Read every word of Chapter 8 (Exceptional Control Flow) in your textbook.
 * 
 * • Use the trace files to guide the development of your shell. Starting with trace01.txt, make
 *   sure that your shell produces the identical output as the reference shell. Then move on to trace file
 *   trace02.txt, and so on.
 * 
 * • The waitpid, kill, fork, execve, setpgid, and sigprocmask functions will come in very
 *   handy. The WUNTRACED and WNOHANG options to waitpid will also be useful.
 *
 * • When you implement your signal handlers, be sure to send SIGINT and SIGTSTP signals to the 
 *   entire foreground process group, using "-pid" instead of "pid" in the argument to the kill function.
 *   The sdriver.pl program tests for this error.
 * 
 * • One of the tricky parts of the assignment is deciding on the allocation of work between the waitfg
 *   and sigchld handler functions. We recommend the following approach:
 *   - In waitfg, use a busy loop around the sleep function.
 *   - In sigchld handler, use exactly one call to waitpid.
 *   While other solutions are possible, such as calling waitpid in both waitfg and sigchld handler,
 *   these can be very confusing. It is simpler to do all reaping in the handler.
 * 
 * • In eval, the parent must use sigprocmask to block SIGCHLD signals before it forks the child,
 *   and then unblock these signals, again using sigprocmask after it adds the child to the job list by
 *   calling addjob. Since children inherit the blocked vectors of their parents, the child must be sure
 *   to then unblock SIGCHLD signals before it execs the new program.
 *   The parent needs to block the SIGCHLD signals in this way in order to avoid the race condition where
 *   the child is reaped by sigchld handler (and thus removed from the job list) before the parent
 *   calls addjob.
 * 
 * • Programs such as more, less, vi, and emacs do strange things with the terminal settings. Don't
 *   run these programs from your shell. Stick with simple text-based programs such as /bin/ls,
 *   /bin/ps, and /bin/echo.
 * 
 * • When you run your shell from the standard Unix shell, your shell is running in the foreground process
 *   group. If your shell then creates a child process, by default that child will also be a member of the
 *   foreground process group. Since typing ctrl-c sends a SIGINT to every process in the foreground
 *   group, typing ctrl-c will send a SIGINT to your shell, as well as to every process that your shell
 *   created, which obviously isn't correct.
 *   Here is the workaround: After the fork, but before the execve, the child process should call
 *   setpgid(0, 0), which puts the child in a new process group whose group ID is identical to the
 *   child's PID. This ensures that there will be only one process, your shell, in the foreground process
 *   group. When you type ctrl-c, the shell should catch the resulting SIGINT and then forward it to the
 *   appropriate foreground job (or more precisely, the process group that contains the foreground job.)
 * 
 *********************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024     /* max line size */
#define MAXARGS     128     /* max args on a command line */
#define MAXJOBS      16     /* max jobs at any point in time */
#define MAXJID    1<<16     /* max job ID */

/* Job states */
#define UNDEF 0             /* undefined */
#define FG 1                /* running in foreground */
#define BG 2                /* running in background */
#define ST 3                /* stopped */

/********************************************************************
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 *******************************************************************/

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

//***********************************************************************************************************************
//***********************************************************************************************************************
/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_command(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
//***********************************************************************************************************************

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

// Helper routines that manipulate the job list
void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

// Other helper routines
void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/********************************************************************
 * main - The shell's main routine
 *******************************************************************/
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {
        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
        {
            app_error("fgets error");
        }
        if (feof(stdin)) /* End of file (ctrl-d) */
        {
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}

/****************************************************************************************************************************************
 ****************************************************************************************************************************************
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
 ***************************************************************************************************************************************/
void eval(char *cmdline) 
{
    char *argv[MAXARGS];    // Argument list execve()
    int bg;                 // Should the job run in bg or fg
    pid_t pid;              // Process id
    sigset_t mask;          // Signal set to block certain signals
        
    sigemptyset(&mask);
    bg = parseline(cmdline, argv);
    if (argv[0] == NULL)
    {
        return;                                     /* Ignore empty lines */
    }

    if (!builtin_command(argv))                     /* No need to fork buildin command */
    {
        // Fork & Exec Test Line
        //printf("Forking & Execing\n");

        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, NULL);        /* Block SIGCHLD */

        if ((pid = fork()) == 0)                    /* Child runs user job */
        {
            sigprocmask(SIG_UNBLOCK, &mask, NULL);  /* Unblock SIGCHLD in child */
            setpgid(0,0);   /* Puts the child in a new process group, GID = PID */
            if(execve(argv[0], argv, environ) < 0)
            {
                fprintf(stderr, "%s: Command not found\n", argv[0]);
                exit(0);
            }
        }
        // Adds the child to job list
        addjob(jobs, pid, (bg?BG:FG), cmdline);
        
        // Unblock SIGCHLD
        sigprocmask(SIG_UNBLOCK, &mask, NULL);

        /* Parent waits for fdreground job to terminate */
        // If not a background process, wait for child
        if (!bg)
        {
            // Parent waits for foreground job to terminate
            waitfg(pid);
        }
        // Else a background process, no need to wait
        else
        {
            // Show information of background job
            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
        }
    }
    return;
}

/********************************************************************
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 *******************************************************************/
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE];     /* holds local copy of command line */
    char *buf = array;              /* ptr that traverses command line */
    char *delim;                    /* points to first space delimiter */
    int argc;                       /* number of args */
    int bg;                         /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';       /* replace trailing '\n' with space */
    while (*buf && (*buf == ' '))   /* ignore leading spaces */
	buf++;

    // Build the argv list
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
	    delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' '))
        {
            // Ignore spaces
            buf++;
        }
        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;
    
    if (argc == 0)
    {
        // Ignore blank line
	    return 1;
    }

    // Should the job run in the background?
    if ((bg = (*argv[argc-1] == '&')) != 0)
    {
	    argv[--argc] = NULL;
    }

    return bg;
}

/****************************************************************************************************************************************
 ****************************************************************************************************************************************
 * builtin_command - If the user has typed a built-in command then execute it immediately.
 ***************************************************************************************************************************************/
int builtin_command(char **argv) 
{    
    // If command is quit
    if (!strcmp(argv[0], "quit"))
    {
        // Exit the shell
        exit(0);
    }
    // Else if command is fg or bg
    else if (!strcmp(argv[0], "fg") || !strcmp(argv[0], "bg"))
    {
        // Handle the fg or bg command
        do_bgfg(argv);
        return 1;
    }
    // Else if command is jobs
    else if (!strcmp(argv[0], "jobs"))
    {
        // List current jobs
        listjobs(jobs);
        return 1;
    }
    
    return 0;     /* not a builtin command */
}

/****************************************************************************************************************************************
 ****************************************************************************************************************************************
 * do_bgfg - Execute the builtin bg and fg commands
 ***************************************************************************************************************************************/
void do_bgfg(char **argv) 
{
    char *id = argv[1];
    struct job_t *job;
    int i;
    int length;

    if(id == NULL)
    {
        printf("%s command requires PID or %%jobid argument\n",argv[0]);
        return;
    }

    // Identified by JID
    if(id[0] == '%') 
    {
        // Skip the '%'
        id++;
        length = strlen(id);
        for (i = 0; i < length; i++)
        {
            // Check if ID are digit numbers
            if(!isdigit(id[i]))
            {
                printf("%s: argument must be a PID or %%jobid\n", argv[0]);
                return;
            }
        }
        job = getjobjid(jobs, atoi(id));
        if(job == NULL) {
            printf("%%%d: No such job\n", atoi(id));
            return;
        }
    }
    // Else identified by PID 
    else
    {
        length = strlen(id);
        for (i = 0; i < length; i++) 
        {  
            // Check if ID are digit numbers
            if(!isdigit(id[i])) 
            {
                printf("%s: argument must be a PID or %%jobid\n", argv[0]);
                return;
            }
        }
        job = getjobpid(jobs, atoi(id));
        if(job == NULL)
        {
            printf("(%d): No such process\n", atoi(id));
            return;
        }
    }

    // Send SIGCONT to the job
    kill(-(job->pid), SIGCONT);

    // If command is foreground (fg)
    if(!strcmp(argv[0], "fg"))
    {  
        // Waits until foreground job terminates
        job->state = FG;
        waitfg(job->pid);
    }
    // Else command is backgorund (bg)
    else
    {
        // Shows information of background job
        job->state = BG;
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
    }
    return;
}

/****************************************************************************************************************************************
 **************************************************************************************************************************************** 
 * waitfg - Block until process pid is no longer the foreground process
 ***************************************************************************************************************************************/
void waitfg(pid_t pid)
{
    // In waitfg, use a busy loop around the sleep function.
    while (1)
    {
        if (pid != fgpid(jobs))
        {
            if (verbose) 
            {
                printf("waitfg: Process (%d) no longer the fg process\n", (int) pid);
            }
            
            break;
        }

        else
        {
            sleep(1);
        }
    }
    
    return;
}

/////////////////////////////////////////////////
// Signal handlers
/////////////////////////////////////////////////

/****************************************************************************************************************************************
 ****************************************************************************************************************************************
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 ***************************************************************************************************************************************/
void sigchld_handler(int sig) 
{
    pid_t pid;
    int status;
    int olderrno = errno;

    while((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0)
    {
        if(WIFEXITED(status))
        { 
            // Process terminated normally
            deletejob(jobs, pid);
        }

        if(WIFSIGNALED(status))
        {
            // Process terminated by signals (ctrl-c)
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            deletejob(jobs, pid);
        }

        if(WIFSTOPPED(status))
        {
            // Process stopped by signals (ctrl-z)
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
            struct job_t *job = getjobpid(jobs, pid);
            job->state = ST;
        }
    }

    if(pid < 0 && errno != ECHILD) {
        unix_error("waitpid error");
    }

    // Set errno to olderrno
    errno = olderrno;

    return;
}

/****************************************************************************************************************************************
 ****************************************************************************************************************************************
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 ***************************************************************************************************************************************/
void sigint_handler(int sig) 
{
    int olderrno = errno;
    pid_t pid = fgpid(jobs);
    
    // Do nothing if no FG job exist
    if(pid != 0)
    {
        if(kill(-pid, SIGINT) < 0)
        {
            // Send signal to entire foreground process group
            unix_error("sigint error");
        }
    }

    // Set errno to olderrno
    errno = olderrno;

    return;
}

/****************************************************************************************************************************************
 ****************************************************************************************************************************************
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 ***************************************************************************************************************************************/
void sigtstp_handler(int sig) 
{
    int olderrno = errno;
    pid_t pid = fgpid(jobs);

    // Do nothing if no foreground job exists
    if(pid != 0) 
    {
        if(kill(-pid, SIGTSTP) < 0)
        {
            // Send signal to entire foreground process group
            unix_error("sigint error");
        }
    }

    // Set errno to olderrno
    errno = olderrno;
    
    return;
}

/////////////////////////////////////////////////
// End signal handlers
/////////////////////////////////////////////////
// Helper routines that manipulate the job list
/////////////////////////////////////////////////

/********************************************************************
 * clearjob - Clear the entries in a job struct
 *******************************************************************/
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}
/********************************************************************
 * initjobs - Initialize the job list 
 *******************************************************************/
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
	    clearjob(&jobs[i]);
    }
}
/********************************************************************
 * maxjid - Returns largest allocated job ID
 *******************************************************************/
int maxjid(struct job_t *jobs)
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
    {
	    if (jobs[i].jid > max)
	    {
            max = jobs[i].jid;
        }
    }

    return max;
}
/********************************************************************
 * addjob - Add a job to the job list 
 *******************************************************************/
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;

            if (nextjid > MAXJOBS)
            {
                nextjid = 1;
            }

            strcpy(jobs[i].cmdline, cmdline);

            if(verbose)
            {
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }

            return 1;
        }
    }

    // Print error message for creation of too many jobs
    printf("Attempted to create too many jobs\n");

    return 0;
}
/********************************************************************
 * deletejob - Delete a job whose PID=pid from the job list
 *******************************************************************/
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
    {
	    return 0;
    }

    for (i = 0; i < MAXJOBS; i++)
    {
	    if (jobs[i].pid == pid)
        {
	        clearjob(&jobs[i]);
	        nextjid = maxjid(jobs)+1;
	        return 1;
	    }
    }

    return 0;
}
/********************************************************************
 * fgpid - Return PID of current foreground job, 0 if no such job
 *******************************************************************/
pid_t fgpid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
	    if (jobs[i].state == FG)
        {
	        return jobs[i].pid;
        }
    }

    return 0;
}
/********************************************************************
 * getjobpid  - Find a job (by PID) on the job list
 *******************************************************************/
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
    {
	    return NULL;
    }

    for (i = 0; i < MAXJOBS; i++)
	{
        if (jobs[i].pid == pid)
	    {
            return &jobs[i];
        }
    }

    return NULL;
}
/********************************************************************
 * getjobjid  - Find a job (by JID) on the job list
 *******************************************************************/ 
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
    {
	    return NULL;
    }

    for (i = 0; i < MAXJOBS; i++)
	{
        if (jobs[i].jid == jid)
	    {
            return &jobs[i];
        }
    }

    return NULL;
}
/********************************************************************
 * pid2jid - Map process ID to job ID
 *******************************************************************/
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
    {
	    return 0;
    }

    for (i = 0; i < MAXJOBS; i++)
	{
        if (jobs[i].pid == pid)
        {
            return jobs[i].jid;
        }
    }

    return 0;
}
/********************************************************************
 * listjobs - Print the job list
 *******************************************************************/ 
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state)
            {
                case BG: 
                    printf("Running ");
                    break;
                case FG: 
                    printf("Foreground ");
                    break;
                case ST: 
                    printf("Stopped ");
                    break;
                default:
                    printf("listjobs: Internal error: job[%d].state=%d ", 
                    i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/////////////////////////////////////////////////
// end job list helper routines
/////////////////////////////////////////////////
// Other helper routines
/////////////////////////////////////////////////

/********************************************************************
 * usage - print a help message
 *******************************************************************/
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/********************************************************************
 * unix_error - unix-style error routine
 *******************************************************************/
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/********************************************************************
 * app_error - application-style error routine
 *******************************************************************/
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/********************************************************************
 * Signal - wrapper for the sigaction function
 *******************************************************************/
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/********************************************************************
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 *******************************************************************/
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}