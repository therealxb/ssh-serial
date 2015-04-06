/* ssh-serial -- a ssh service for serial port access.
 *
 * This program is not part of the OpenSSH or Portable SSH software
 * and has no relationship with those projects.
 *
 * Copyright (C) Glen David Turner of Semaphore, South Australia.
 * <http://www.gdt.id.au/~gdt/ssh-serial/>
 *
 * $Id$
 */

static const char rcsid[] = "@(#)$Id$";

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <poll.h>

#define PROGRAM_NAME "ssh-serial"
#define PROGRAM_EXIT_SUCCESS 0
#define PROGRAM_EXIT_FAIL 1
#define PROGRAM_EXIT_SYNTAX 2
/* stderr does not appear in the output sent to a subsystem. */
#define PROGRAM_STDERR stdout

/* Slots in poll() file descriptor array. */
#define POLLFD_STDIN 0
#define POLLFD_STDOUT 1
#define POLLFD_SERIALINOUT 2
/* Number of slots in file descriptor array. */
#define POLLFD_SIZE 3

/* Options. */
struct options_t {
  /* Name of serial device, /dev/ttyS0 or similar. */
  char *device_name;
  /* Bits per second, in termio and string formats.
   * Input and output distinct to allow 1200/75bps.
   */
  speed_t input_speed;
  speed_t output_speed;
  char *input_speed_name;
  char *output_speed_name;
  /* Obey Data Carrier Detect and hang up when not asserted? */
  int dcd;
  /* Obey CTS/RTS hardware flow control? */
  int ctsrts;
};

struct speed_list_t {
  char *text;
  speed_t speed;
};
static struct speed_list_t speed_list[] = {
  {     "50",     B50 },
  {     "75",     B75 },
  {    "110",    B110 },
  {    "134",    B134 },  /* Actually 134.5bps. */
  {    "134.5",  B134 },
  {    "150",    B150 },
  {    "200",    B200 },
  {    "300",    B300 },
  {    "600",    B600 },
  {   "1200",   B1200 },
  {   "1800",   B1800 },
  {   "2400",   B2400 },
  {   "4800",   B4800 },
  {   "9600",   B9600 },
  {  "19200",  B19200 },
  {  "38400",  B38400 },
  {  "57600",  B57600 },
  { "115200", B115200 },
  { "230400", B230400 },
  {     NULL,      B0 }   /* End of list. */
};


#define die_printf(EXIT_CODE, FORMAT, ...) do_die_printf(__FILE__, __LINE__, __func__, EXIT_CODE, FORMAT, ##__VA_ARGS__)

__attribute__((noreturn, format(printf, 5, 6)))
void
do_die_printf(const char* file_name,
              int file_line,
              const char *function_name,
              int exit_code,
	      const char *format,
	      ...)
{
  va_list args;
  
  (void)fprintf(PROGRAM_STDERR,
                PROGRAM_NAME ": ");
  if (errno) {
    (void)fprintf(PROGRAM_STDERR,
                  "%s: ",
		  strerror(errno));
  }
  va_start(args, format);
  (void)vfprintf(PROGRAM_STDERR,
		 format,
		 args);
  va_end(args);
  if (errno) {
    (void)fprintf(PROGRAM_STDERR,
                  PROGRAM_NAME ": (Program was in function %s() of file %s line %d.)\n",
                  function_name,
                  file_name,
                  file_line);
  }

  exit(exit_code);
}


void
parse_speed(char *text,
	    speed_t *speed_p)
{
  struct speed_list_t *p;

  for (p = speed_list; p->text != NULL; p++) {
    if (0 == strcmp(text, p->text)) {
      *speed_p = p->speed;
      break;
    }
  }
  if (p->text == NULL) {
    die_printf(PROGRAM_EXIT_SYNTAX,
               "Paramater \"%s\" is not an available RS-232 speed.\n",
               text);
  }
}



void
parse_bool(char *text,
	   int *flag)
{
  char c;

  if (0 == strcmp(text, "1") ||
      0 == strcmp(text, "y") ||
      0 == strcmp(text, "Y")) {
    *flag = 1;
    return;
  }
  if (0 == strcmp(text, "0") ||
      0 == strcmp(text, "n") ||
      0 == strcmp(text, "N")) {
    *flag = 0;
    return;
  }
  die_printf(PROGRAM_EXIT_SYNTAX,
             "\"%s\" is not boolean, use \"1\" for yes, \"0\" for no.\n",
             (text == NULL) ? "???" : text);
}


static char *version_list[] = {
  PROGRAM_NAME ":\n"
  "   A subsystem for ssh servers to allow incoming ssh connections to attach "
    "to",
  " a serial device on the server. That serial port might in turn connect to "
    "the",
  " serial console of another machine, turning the ssh server into a simple "
    "console",
  " server.\n",
  "Copyright © Glen Turner of Semaphore, South Australia, 2015.",
  "   " PROGRAM_NAME " is free software: you can redistribute it and/or modify "
    "it under",
  " the terms of version 2 of the GNU General Public License as published by "
    "the",
  " Free Software Foundation.",
  "   " PROGRAM_NAME " is distributed in the hope that it will be useful, but "
    "WITHOUT ANY",
  " WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS "
    "FOR A",
  " PARTICULAR PURPOSE. See the GNU General Public License for more details.",
  "   You should have received a copy of the GNU General Public License "
    "version 2.0",
  " along with " PROGRAM_NAME ". If not, "
    "see <http://www.gnu.org/licenses/gpl-2.0.html>\n",
  NULL   /* End of list. */
};


void
parse_version(struct options_t *options) {
  char **p;
  struct speed_list_t *speed_p;
  int comma = 0;

  for (p = version_list; *p != NULL; p++) {
    puts(*p);
  }

  printf(PROGRAM_NAME " release %s\n\n", rcsid);

  printf("Command line options and their values:\n");
  printf("  --device %s\n",
	 options->device_name);
  printf("  --bits-per-second-input %s (speed_t %d)\n",
	 options->input_speed_name,
	 (int)options->input_speed);
  printf("  --bits-per-second-output %s (speed_t %d)\n",
	 options->output_speed_name,
	 (int)options->output_speed);
  printf("  --data-carrier-detect %d\n",
	 options->dcd);
  printf("  --hardware-handshaking %d\n",
	 options->ctsrts);
  printf("Available bit-per-second values:");
  for (speed_p = speed_list;
       speed_p->text != NULL;
       speed_p++) {
    printf(comma ? ", " : " ");
    comma = 1;
    printf("%s",
	   speed_p->text);
  }
  printf(".\n");

  exit(PROGRAM_EXIT_SUCCESS);
}


static char *help_list[] = {
  PROGRAM_NAME " usage and options:\n",
  " " PROGRAM_NAME " [-b BPS] [-c {0|1}] [-d DEVICE] [-h {0|1}]\n",
  "  --bits-per-second BPS   -b BPS\n",
  "      Speed of input and output data through RS-232 interface, in\n,"
  "      bits-per-second.\n",
  "      Optional, default --bits-per-second 9600.\n",
  "  --bits-per-second-input BPS   -i BPS\n",
  "      Speed of input data through RS-232 interface, in bits-per-second.\n",
  "      Optional, default --bits-per-second 9600.\n",
  "  --bits-per-second-output BPS   -j BPS\n",
  "      Speed of output data through RS-232 interface, in bits-per-second.\n",
  "      Optional, default --bits-per-second 9600.\n",
  "  --data-carrier-detect {0|1}   -c {0|1}\n",
  "      When 0: ignore RS-232 Data Carrier Detect.\n",
  "      When 1: obey RS-232 Data Carrier Detect, not connecting until DCD "
    "is\n",
  "              asserted, clearing down session when DCD not asserted.\n",
  "      Optional, default --data-carrier-detect 1.\n",
  "  --device DEVICE   -d DEVICE\n",
  "      Name of serial device file.\n",
  "      Optional, default --device /dev/ttyS0.\n",
  "  --hardware-handshaking {0|1}   -h {0|1}\n",
  "      When 0: no RS-232 Clear to Send/Ready to Send hardware handshaking.\n",
  "      When 1: use RS-232 CTS/RTS hardware handshaking to prevent "
    "character\n",
  "              overruns.\n",
  "      Optional, default --hardware-handshaking 1.\n",
  " " PROGRAM_NAME " -V\n",
  "  --version   -V\n",
  "      Display version information, copyright license, available BPS "
    "values, then\n",
  "      exit.\n",
  NULL    /* End of list. */
};


void
parse_help(FILE *f) {
  char **p;

  for (p = help_list; *p != NULL; p++) {
    fputs(*p, f);
  }
}


void
parse_options(int argc,
	      char *argv[],
	      struct options_t *options)
{
  static struct option getopt_list[] = {
    { "bits-per-second", required_argument, NULL, 'b' },
    { "bits-per-second-input", required_argument, NULL, 'i' },
    { "bits-per-second-output", required_argument, NULL, 'j' },
    { "data-carrier-detect", required_argument, NULL, 'c' },
    { "device", required_argument, NULL, 'd' },
    { "hardware-handshaking", required_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'V' },
    { 0, 0, 0, 0 }   /* End of list. */
  };
  int opt;
  int opt_index = 0;

  /* Defaults. */
  options->device_name = "/dev/ttyS0";
  options->input_speed_name = "9600";
  parse_speed(options->input_speed_name, &(options->input_speed));
  options->output_speed_name = "9600";
  parse_speed(options->output_speed_name, &(options->output_speed));
  options->dcd = 1;
  options->ctsrts = 1;

  while (-1 != (opt = getopt_long(argc,
				  argv,
				  "b:c:d:h:V",
				  getopt_list,
				  &opt_index))) {
    switch (opt) {
    case 'b':
      options->input_speed_name = optarg;
      parse_speed(optarg, &(options->input_speed));
      options->output_speed_name = options->input_speed_name;
      options->output_speed = options->input_speed;
      break;
    case 'c':
      parse_bool(optarg, &(options->dcd));
      break;
    case 'd':
      options->device_name = optarg;
      break;
    case 'h':
      parse_bool(optarg, &(options->ctsrts));
      break;
    case 'i':
      options->input_speed_name = optarg;
      parse_speed(optarg, &(options->input_speed));
      break;
    case 'j':
      options->output_speed_name = optarg;
      parse_speed(optarg, &(options->output_speed));
      break;      
    case 'V':
      parse_version(options);
      break;
    default:
      parse_help(PROGRAM_STDERR);
      exit(PROGRAM_EXIT_SYNTAX);
    }
  }
}

/* Set up the serial port.
 *
 * The configuration of serial ports differs substantially for UNIX
 * variants. This code is exceoptionally specific to Linux.
 */
void
setup_serial(int f,
	     struct options_t *options)
{
  struct termios t;

  /* Read current settings. */
  if (tcgetattr(f, &t)) {
    die_printf(PROGRAM_EXIT_FAIL,
	       "Failed getting attributes from serial port %s.\n",
	       options->device_name);
  }

  /* Set "raw mode", see termios(3). */
  t.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
		 ICRNL | IXON);
  t.c_oflag &= ~OPOST;
  t.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  t.c_cflag &= ~(CSIZE | PARENB);
  t.c_cflag |= CS8;

  /* Update speed from options. */
  if (cfsetospeed(&t, options->output_speed)) {
    die_printf(PROGRAM_EXIT_FAIL,
	       "Failed to set output speed to %s for serial port %s.\n",
	       options->output_speed_name,
	       options->device_name);
  }
  if (cfsetispeed(&t, options->input_speed)) {
    die_printf(PROGRAM_EXIT_FAIL,
	       "Failed to set input speed to %s for serial port %s.\n",
	       options->input_speed_name,
	       options->device_name);
  }

  /* Update Obey DCD from options. */
  t.c_cflag = (t.c_cflag & ~CLOCAL);
  if (0 == options->dcd) {
     t.c_cflag = t.c_cflag | CLOCAL;
  }

  /* Update CTS/RTS handshaking from options. */
  t.c_cflag = (t.c_cflag & ~CRTSCTS);
  if (options->ctsrts) {
    t.c_cflag = t.c_cflag | CRTSCTS;
  }

  /* Drop DTR on exit. */
  t.c_cflag = (t.c_cflag & ~HUPCL) | HUPCL;

  /* Set serial port to assembled settings. */
  if (tcsetattr(f, TCSANOW, &t)) {
    die_printf(PROGRAM_EXIT_FAIL,
	       "Failed to establish settings for serial port %s.\n",
	       options->device_name);
  }
}


int
main(int argc,
     char *argv[])
{
  int f;
  int disconnected;
  int ready;
  static struct options_t options;
  struct pollfd pollfds[3];
  short stdin_revents;
  short stdout_revents;
  short serialinout_revents;
  unsigned long timeout_count = 0UL;
  unsigned long stdin_count = 0UL;
  unsigned long stdout_count = 0UL;
  unsigned long serialin_count = 0UL;
  unsigned long serialout_count = 0UL;

  parse_options(argc, argv, &options);

  f = open(options.device_name,
	   O_NOCTTY);
  if (-1 == f) {
    die_printf(PROGRAM_EXIT_FAIL,
	       "Failed opening serial device %s.\n",
	       options.device_name);
  }

  setup_serial(f, &options);

  pollfds[POLLFD_STDIN].fd = fileno(stdin);
  pollfds[POLLFD_STDIN].events = POLLIN | POLLRDHUP;
  pollfds[POLLFD_STDIN].revents = 0;
  pollfds[POLLFD_STDOUT].fd = fileno(stdout);
  pollfds[POLLFD_STDOUT].events = POLLOUT | POLLERR | POLLHUP;
  pollfds[POLLFD_STDOUT].revents = 0;
  pollfds[POLLFD_SERIALINOUT].fd = f;
  pollfds[POLLFD_SERIALINOUT].events = POLLIN | POLLOUT | POLLERR | POLLHUP;
  pollfds[POLLFD_SERIALINOUT].revents = 0;
  disconnected = 0;
  while (!disconnected) {
    ready = poll(pollfds,
		 POLLFD_SIZE,
		 -1);
    if (-1 == ready) {
      die_printf(PROGRAM_EXIT_FAIL,
	 	 "Failed waiting for I/O on serial device %s.\n",
		 options.device_name);
    }
    if (0 == ready) {
      timeout_count++;
    }
    if (ready > 0) {
      stdin_revents = pollfds[POLLFD_STDIN].revents;
      if (stdin_revents) {
	if (POLLRDHUP & stdin_revents) {
	  /* POLLRDHUP - neighbour has hung up
	   * No more data will be available,so stop monitoring this file.
	   */
	  pollfds[POLLFD_STDIN].fd = -pollfds[POLLFD_STDIN].fd;
	}
	if ((POLLIN | POLLPRI) & stdin_revents) {
	  /* POLLIN - data to be read
	   * POLLPRI - urgent data to be read
	   * Read the data into the network->serial buffer.
	   */
	  stdin_count++;
	}
      }
      
      stdout_revents = pollfds[POLLFD_STDOUT].revents;
      if (stdout_revents) {
	if (POLLHUP & stdout_revents) {
	  /* POLLHUP - hang up
	   */
	  pollfds[POLLFD_STDOUT].fd = -pollfds[POLLFD_STDOUT].fd;
	}
	if ((POLLERR | POLLNVAL) & stdout_revents) {
	  /*  POLLERR - error
	   *  POLLNVAL - invalid, fd not open()ed
	   */
	  die_printf(PROGRAM_EXIT_FAIL,
		     "Failed polling output on network.\n");
	}
	if (POLLOUT & stdout_revents) {
	  /* POLLOUT - write() will not block
	   * Write from serial->netowrk buffer to stdout.
	   */
	  stdout_count++;
	}
      }

      serialinout_revents = pollfds[POLLFD_SERIALINOUT].revents;
      if (serialinout_revents) {
	/* Errors. */
	if ((POLLERR | POLLNVAL) & serialinout_revents) {
	  /*  POLLERR - error
	   *  POLLNVAL - invalid, fd not open()ed
	   */
	  die_printf(PROGRAM_EXIT_FAIL,
		     "Failed polling output on serial.\n");
	}
	/* Begin to drain. */
	if (POLLHUP & serialinout_revents) {
	  /* POLLHUP - hang up
	   */
	  pollfds[POLLFD_SERIALINOUT].fd = -pollfds[POLLFD_SERIALINOUT].fd;
	}
	if (POLLRDHUP & serialinout_revents) {
	  /* POLLRDHUP - neighbour has hung up
	   * No more data will be available,so stop monitoring this file.
	   */
	  pollfds[POLLFD_SERIALINOUT].fd = -pollfds[POLLFD_SERIALINOUT].fd;
	}
	/* I/O possible. */
	if ((POLLIN | POLLPRI) & serialinout_revents) {
	  /*  POLLIN - data to read.
	   */
	  serialin_count++;
	}
	if (POLLOUT & serialinout_revents & serialinout_revents) {
	  /* POLLOUT - write() will not block.
	   */
	  serialout_count++;
	}
      }
      /* If any fd has closed and buffers are empty then exit.
       */
      
      /* Check that we are making progress draining buffers,
       * otherwise if any fd has been closed then exit.
       */
    }
  }

  if (close(f)) {
    die_printf(PROGRAM_EXIT_FAIL,
	       "Failed closing serial device %s.\n",
	       options.device_name);
  }

  return PROGRAM_EXIT_SUCCESS;
}
