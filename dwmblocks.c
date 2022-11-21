#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#ifndef NO_X
#include<X11/Xlib.h>
#endif
#ifdef __OpenBSD__
#define SIGPLUS			SIGUSR1+1
#define SIGMINUS		SIGUSR1-1
#else
#define SIGPLUS			SIGRTMIN
#define SIGMINUS		SIGRTMIN
#endif
#define LENGTH(X)               (sizeof(X) / sizeof (X[0]))
#define CMDLENGTH		50
#define MIN( a, b ) ( ( a < b) ? a : b )
#define STATUSLENGTH (LENGTH(blocks) * CMDLENGTH + 1)

typedef struct {
	char* icon;
	char* command;
	char* args;
	unsigned int interval;
	unsigned int signal;
} Block;
#ifndef __OpenBSD__
void dummysighandler(int num);
#endif
void sighandler(int num);
void getcmds(int time);
void getsigcmds(unsigned int signal);
void setupsignals();
void sighandler(int signum);
int getstatus(char *str, char *last);
void statusloop();
void termhandler();
void pstdout();
#ifndef NO_X
void setroot();
static void (*writestatus) () = setroot;
static int setupX();
static Display *dpy;
static int screen;
static Window root;
#else
static void (*writestatus) () = pstdout;
#endif

#include "limits.h"
#include "blocks.h"

#define NETWORK_RX_BYTES_TOTAL "/sys/class/net/%s/statistics/rx_bytes"
#define NETWORK_TX_BYTES_TOTAL "/sys/class/net/%s/statistics/tx_bytes"

static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[2][STATUSLENGTH];
static int statusContinue = 1;

char* transfer_speed(int receive, const char* args, ...) {
	char path[PATH_MAX];
	// sprintf(path, NETWORK_RX_BYTES_TOTAL, args);
	if (receive) {
		sprintf(path, "/sys/class/net/%s/statistics/rx_bytes", args);
	} else {
		sprintf(path, "/sys/class/net/%s/statistics/tx_bytes", args);
	}
	
	static long long rx_bytes_total;
	static long long tx_bytes_total;

	long long previous_bytes = receive > 0 ? rx_bytes_total : tx_bytes_total;

	FILE* fp = fopen(path, "r");
	if (receive) {
		fscanf(fp, "%lld", &rx_bytes_total);
	} else {
		fscanf(fp, "%lld", &tx_bytes_total);
	}
	fclose(fp);
	
	double scaled = ((receive > 0 ? rx_bytes_total : tx_bytes_total) - previous_bytes) * 8;
	size_t i;
	
	char* prefixes[] = { " bps", "Kbps", "Mbps", "Gbps", "Tbps", "Pbps" };
	for(i = 0; i < 6 && scaled >= 1000; ++i)
		scaled /= 1000;

	char* out_buf = malloc(50 * sizeof(char));
	sprintf(out_buf, "%5.1f%s", scaled, prefixes[i]);
	return out_buf;
}

char* execute_builtin(const char* function_name, const char* args, ...) {
	if (strcmp(function_name, "rx_speed") == 0) {
		return transfer_speed(1, args);
	} else if (strcmp(function_name, "tx_speed") == 0) {
		return transfer_speed(0, args);
	} 
	return NULL;
}

//opens process *cmd and stores output in *output
void getcmd(const Block *block, char *output)
{
	strcpy(output, block->icon);
	int i = strlen(block->icon);

	// check if command is a shell command or a built in function
	char* res;
	if((res = execute_builtin(block->command, block->args)) != NULL) {
		strcpy(output+i, res);
		// free(res);		
		return;
	} 
	FILE *cmdf = popen(block->command, "r");
	if (!cmdf)
		return;
	// execute command
	fgets(output+i, CMDLENGTH-i-delimLen, cmdf);
			i = strlen(output);
	if (i == 0) {
		// return if block and command output are both empty
		pclose(cmdf);
		return;
	}
	// only chop off newline if one is present at the end
	i = output[i-1] == '\n' ? i-1 : i;
	if (delim[0] != '\0') {
		strncpy(output+i, delim, delimLen); 
	}
	else
		output[i++] = '\0';
	pclose(cmdf);
}

void getcmds(int time)
{
	const Block* current;
	for (unsigned int i = 0; i < LENGTH(blocks); i++) {
		current = blocks + i;
		if ((current->interval != 0 && time % current->interval == 0) || time == -1)
			getcmd(current,statusbar[i]);
	}
}

void getsigcmds(unsigned int signal)
{
	const Block *current;
	for (unsigned int i = 0; i < LENGTH(blocks); i++) {
		current = blocks + i;
		if (current->signal == signal)
			getcmd(current,statusbar[i]);
	}
}

void setupsignals()
{
#ifndef __OpenBSD__
	    /* initialize all real time signals with dummy handler */
    for (int i = SIGRTMIN; i <= SIGRTMAX; i++)
        signal(i, dummysighandler);
#endif

	for (unsigned int i = 0; i < LENGTH(blocks); i++) {
		if (blocks[i].signal > 0)
			signal(SIGMINUS+blocks[i].signal, sighandler);
	}

}

int getstatus(char *str, char *last)
{
	strcpy(last, str);
	str[0] = '\0';
	for (unsigned int i = 0; i < LENGTH(blocks); i++)
		strcat(str, statusbar[i]);
	str[strlen(str)-strlen(delim)] = '\0';
	return strcmp(str, last);//0 if they are the same
}

#ifndef NO_X
void setroot()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only set root if text has changed.
		return;
	XStoreName(dpy, root, statusstr[0]);
	XFlush(dpy);
}

int setupX()
{
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "dwmblocks: Failed to open display\n");
		return 0;
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	return 1;
}
#endif

void pstdout()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only write out if text has changed.
		return;
	printf("%s\n",statusstr[0]);
	fflush(stdout);
}


void statusloop()
{
	setupsignals();
	int i = 0;
	getcmds(-1);
	while (1) {
		getcmds(i++);
		writestatus();
		if (!statusContinue)
			break;
		sleep(1.0);
	}
}

#ifndef __OpenBSD__
/* this signal handler should do nothing */
void dummysighandler(int signum)
{
    return;
}
#endif

void sighandler(int signum)
{
	getsigcmds(signum-SIGPLUS);
	writestatus();
}

void termhandler()
{
	statusContinue = 0;
}

int main(int argc, char** argv)
{
	for (int i = 0; i < argc; i++) {//Handle command line arguments
		if (!strcmp("-d",argv[i]))
			strncpy(delim, argv[++i], delimLen);
		else if (!strcmp("-p",argv[i]))
			writestatus = pstdout;
	}
#ifndef NO_X
	if (!setupX())
		return 1;
#endif
	delimLen = MIN(delimLen, strlen(delim));
	delim[delimLen++] = '\0';
	signal(SIGTERM, termhandler);
	signal(SIGINT, termhandler);
	statusloop();
#ifndef NO_X
	XCloseDisplay(dpy);
#endif
	return 0;
}
