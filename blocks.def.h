//Modify this file to change what commands output to your statusbar, and recompile using the make command.
static const Block blocks[] = {
	/*Icon*/	/*Command*/		/*Args*/	/*Update Interval*/	/*Update Signal*/
	{" ", "tx_speed",	"enp9s0",	1,	0},
	{"  ", "rx_speed",	"enp9s0",	1,	0},
	{"  ", "checkupdates | wc -l",	NULL,	360,	0},
	{" ", "top -b -n 1 | grep 'Cpu' | awk '{printf \"%4.1f%\", $2}'",	NULL,	2,	0},
	{" ", "sensors | awk '/^Tctl/ {print $2}' | awk -F'+' '{print $2}'",	NULL,	10,	0},
	{" ", "free -h | awk '/^Mem/ { print $3\"/\"$2 }' | sed s/i//g",	NULL,	5,	0},
	{" " , "date '+%H:%M'",		NULL,	60,	0},
};

//sets delimeter between status commands. NULL character ('\0') means no delimeter.
static char delim[] = " ";
static unsigned int delimLen = 5;
