/**
 *  ______  ____     __  __  ____    ____    ____
 * /\  _  \/\  _`\  /\ \/\ \/\  _`\ /\  _`\ /\  _`\
 * \ \ \L\ \ \ \L\ \\ \ \ \ \ \,\L\_\ \ \L\_\ \ \/\ \
 *  \ \  __ \ \  _ <'\ \ \ \ \/_\__ \\ \  _\L\ \ \ \ \
 *   \ \ \/\ \ \ \L\ \\ \ \_\ \/\ \L\ \ \ \L\ \ \ \_\ \
 *    \ \_\ \_\ \____/ \ \_____\ `\____\ \____/\ \____/
 *     \/_/\/_/\/___/   \/_____/\/_____/\/___/  \/___/
 *
 *
 * [A]bused is [B]anks [U]ber-[S]exy-[E]dition [D]aemon
 *
 *  Copyright(C) 2014 by Andreas Bank, andreas.mikael.bank@gmail.com
 *
 * Update 2013-10-01:
 *  by Andreas Bank
 *   Made output more flexible, see usage().
 *
 * Update 2014-01-03:
 *  by Andreas Bank
 *   Added daemon and server functionality, for remote output.
 *
 * Update 2014-01-10:
 *  by Andreas Bank
 *   Added upnp (ssdp) listening support
 *
 * Update 2014-01-23:
 *  by Andreas Bank
 *   Parsing of upnp (ssdp) messages
 *
 * Update 2014-02-04:
 *  by Andreas Bank
 *   Multi-filtering of upnp (ssdp) messages
 *
 * Update 2014-02-05:
 *  by Andreas Bank
 *   Active search for upnp devices
 *
 * Update 2014-02-06
 *  by Andreas Bank
 *   Option to choose interface by name or IP and Option to set the TTL value
 *
 * Update 2014-02-16
 *  by Andreas Bank
 *   findInterface now supports IPv6
 *
 * Update 2014-02-20:
 *  by Andreas Bank
 *   Added option to output as XML
 *
 * Update 2014-02-27:
 *  by Andreas Bank
 *   Added parsing of device info
 *
 * Update 2014-10-03:
 *  by Andreas Bank
 *   MAC retrieval for BSD systems
 *
 * Update 2015-02-03:
 *  by Andreas Bank
 *   MAC retrieval on linux
 *
 * Update 2015-02-04:
 *  by Andreas Bank
 *   Forwarding support
 *
 * Update 2015-02-07:
 *  by Andreas Bank
 *   Normalise printouts
 */

/*
 *  ████████╗ ██████╗ ██████╗  ██████╗
 *  ╚══██╔══╝██╔═══██╗██╔══██╗██╔═══██╗██╗
 *     ██║   ██║   ██║██║  ██║██║   ██║╚═╝
 *     ██║   ██║   ██║██║  ██║██║   ██║██╗
 *     ██║   ╚██████╔╝██████╔╝╚██████╔╝╚═╝
 *     ╚═╝    ╚═════╝ ╚═════╝  ╚═════╝
 *  01010100 01001111 01000100 01001111 00111010
 *
 *  When listening on "all" interfaces (0.0.0.0)
 *  make it join the multicast group on each one.
 */

#define ABUSED_VERSION "0.0.1"

/* Uncomment the line below to enable simulating notifs */
#define DEBUG_MSG___
/* Set the frequency at which the messages will be received */
#define DEBUG_MSG_FREQ___ 0.5 
/* When using simulated messages it is mandatory to
   specify a UPnP device url in DEBUG_MSG_DEVICE___.
   This "UPnP device" can be emulated with a http server (e.g. Apache)
   serving the UPnP-descriptive XML file included in the
   abused repository (<repo>/udhisapi.xml) */
#define DEBUG_MSG_LOCATION_HEADER "http://10.0.0.2:80/udhisapi.xml"

/* Uncomment the line below to enable detailed debug information */
#define DEBUG___

#define DEBUG_COLOR_BEGIN "\x1b[0;32m"
#define ERROR_COLOR_BEGIN "\x1b[0;31m"
#define DEBUG_COLOR_END "\x1b[0;0m"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netdb.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>

#include <net/if.h>
#include <ifaddrs.h>
#include <ctype.h>

#include <errno.h>
#include <signal.h>
#include <time.h>

#ifdef BSD
#include <sys/sysctl.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h> //test if needed?
#include <sys/file.h>
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

/* PORTABILITY */
#define TRUE              1
#define FALSE             0
#define INVALID_SOCKET   -1
#define SOCKET_ERROR     -1

#ifndef SIOCGARP
    #define SIOCGARP SIOCARPIPLL
#endif

#ifdef DEBUG___
  #define DEBUG_PRINT(...)  printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)  do { } while (false)
#endif

#ifdef DEBUG___
  #define PRINT_DEBUG(...)  print_debug(NULL, DEBUG_COLOR_BEGIN, __FILE__, __LINE__, __VA_ARGS__)
  #define PRINT_ERROR(...)  print_debug(stderr, ERROR_COLOR_BEGIN, __FILE__, __LINE__, __VA_ARGS__)
  #define ADD_ALLOC(...)    add_alloc(...)
  #define REMOVE_ALLOC(...) remove_alloc(...)
  #define PRINT_ALLOC()     add_alloc()
#else
  #define PRINT_DEBUG(...)  do { } while (FALSE)
  #define PRINT_ERROR(...)  printf(__VA_ARGS__)
  #define ADD_ALLOC(arg)    do { } while (FALSE)
  #define REMOVE_ALLOC(arg) do { } while (FALSE)
  #define PRINT_ALLOC(arg)  do { } while (FALSE)
#endif

#define DAEMON_PORT           43210   // port the daemon will listen on
#define RESULT_SIZE           1048576 // 1MB for the scan results
#define SSDP_ADDR             "239.255.255.250" // SSDP address
#define SSDP_ADDR6_LL         "ff02::c" // SSDP IPv6 link-local address
#define SSDP_ADDR6_SL         "ff05::c" // SSDP IPv6 site-local address
#define SSDP_PORT             1900 // SSDP port
#define NOTIF_RECV_BUFFER     2048 // notification receive-buffer
#define XML_BUFFER_SIZE       20480 // XML buffer/container string
#define IPv4_STR_MAX_SIZE     16 // aaa.bbb.ccc.ddd
#define IPv6_STR_MAX_SIZE     46 // xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:aaa.bbb.ccc.ddd
#define MAC_STR_MAX_SIZE  18 // 00:40:8C:1A:2B:3C + '\0'
#define PORT_MAX_NUMBER       65535
#define LISTEN_QUEUE_LENGTH   2
#define DEVICE_INFO_SIZE      16384
#define MULTICAST_TIMEOUT     2

#define ANSI_COLOR_GREEN   "\x1b[1;32m"
#define ANSI_COLOR_RED     "\x1b[1;31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* SSDP header types string representations */
#define SSDP_HEADER_HOST_STR        "host"
#define SSDP_HEADER_ST_STR          "st"
#define SSDP_HEADER_MAN_STR         "man"
#define SSDP_HEADER_MX_STR          "mx"
#define SSDP_HEADER_CACHE_STR       "cache"
#define SSDP_HEADER_LOCATION_STR    "location"
#define SSDP_HEADER_OPT_STR         "opt"
#define SSDP_HEADER_01NLS_STR       "01-nls"
#define SSDP_HEADER_NT_STR          "nt"
#define SSDP_HEADER_NTS_STR         "nts"
#define SSDP_HEADER_SERVER_STR      "server"
#define SSDP_HEADER_XUSERAGENT_STR  "x-user-agent"
#define SSDP_HEADER_USN_STR         "usn"
#define SSDP_HEADER_UNKNOWN_STR     "unknown"

/* SSDP header types uchars */
#define SSDP_HEADER_HOST            1
#define SSDP_HEADER_ST              2
#define SSDP_HEADER_MAN             3
#define SSDP_HEADER_MX              4
#define SSDP_HEADER_CACHE           5
#define SSDP_HEADER_LOCATION        6
#define SSDP_HEADER_OPT             7
#define SSDP_HEADER_01NLS           8
#define SSDP_HEADER_NT              9
#define SSDP_HEADER_NTS             10
#define SSDP_HEADER_SERVER          11
#define SSDP_HEADER_XUSERAGENT      12
#define SSDP_HEADER_USN             13
#define SSDP_HEADER_UNKNOWN         0


typedef int SOCKET;
typedef int BOOL;

/* Structs */
typedef struct ssdp_header_struct {
  unsigned char type;
  char *unknown_type;
  char *contents;
  struct ssdp_header_struct *first;
  struct ssdp_header_struct *next;
} ssdp_header_s;

typedef struct ssdp_message_struct {
  char *mac;
  char *ip;
  int  message_length;
  char *datetime;
  char *request;
  char *protocol;
  char *answer;
  char *info;
  unsigned char header_count;
  struct ssdp_header_struct *headers;
} ssdp_message_s;

typedef struct filter_struct {
  char *header;
  char *value;
} filter_s;

typedef struct filters_factory_struct {
  filter_s *filters;
  filter_s *first_filter;
  char *raw_filters;
  unsigned char filters_count;
} filters_factory_s;

typedef struct configuration_struct {
  char*               interface;             /* Interface to use */
  BOOL                run_as_daemon;         /* Run the program as a daemon ignoring control signals and terminal */
  BOOL                run_as_server;         /* Run the program as a server, open a port and wait for incoming connection (requests) */
  BOOL                listen_for_upnp_notif; /* Switch for enabling listening for UPnP (SSDP) notifications */
  BOOL                scan_for_upnp_devices; /* Switch to perform an active UPnP (SSDP) scan */
  BOOL                forward_enabled;       /* Switch to enable forwarding of the scan results */
  BOOL                raw_output;            /* Use raw input instead of parsing it when displaying or forwarding */
  BOOL                fetch_info;            /* Don't try to fetch device info from the url in the "Location" header */
  BOOL                gather;                /* Gather and display discoveries in a table (usable with -a) */
  BOOL                gather_silent;         /* Gather discoveries, do not display anything */
  BOOL                xml_output;            /* Use raw input instead of parsing it when displaying or forwarding */
  unsigned char       ttl;                   /* Time-To-Live value, how many routers the multicast shall propagate through */
  unsigned char       show_interfaces;       /* Show interfaces at start */
  unsigned char       hide_link_locals;      /* Hide link locals in result */
  unsigned char       print_in_color;        /* Print IP in color */
  unsigned char       print_red;             /* Print IP in red color */
  unsigned char       bonjour_only;          /* Print only bojour friendly name, not full url */
  unsigned char       one_line;              /* Print all in one line */
  char               *filter;                /* Grep-like filter string */
  BOOL                use_ipv4;              /* Force the usage of the IPv4 protocol */
  BOOL                use_ipv6;              /* Force the usage of the IPv6 protocol */
  BOOL                quiet_mode;            /* Minimize informative output */
  int                 upnp_timeout;          /* The time to wait for a device to answer a query */
  BOOL                enable_loopback;       /* Enable multicast loopback traffic */
} configuration_s;


/* Globals */
static SOCKET               notif_server_sock = 0;       /* The main socket for listening for UPnP (SSDP) devices (or device answers) */
static SOCKET               notif_client_sock = 0;       /* The sock that we want to ask/look for devices on (unicast) */
static SOCKET               notif_recipient_sock = 0;    /* Notification recipient socket, where the results will be forwarded */
struct sockaddr_storage    *notif_recipient_addr = NULL;
static char                *send_string = NULL;          /* The results of a UPnP scan */
static char                *recv_string = NULL;          /* The string received from the client that has connected (made a request) to this service */
static char                *notif_string = NULL;         /* A buffer for the notifications received while listening for UPnP (SSDP) notifications */
static filters_factory_s   *filters_factory = NULL;      /* The structure contining all the filters information */
static configuration_s     *conf = NULL;
static char                *time_string = NULL;
#ifdef DEBUG___
static int                  alloc_list_size = 0;
static char               **alloc_list = NULL;
#endif

/* Functions */
static void exitSig(int);
static BOOL build_ssdp_message(ssdp_message_s *, char *, char *, int, const char *);
static unsigned char get_header_type(const char *);
static const char *get_header_string(const unsigned int, const ssdp_header_s *);
static int strpos(const char*, const char *);
static int send_stuff(const char *, const char *, const struct sockaddr_storage *, int, int);
static void free_ssdp_message(ssdp_message_s *);
static int fetch_upnp_device_info(const ssdp_message_s *, char *, int);
static void to_xml(const ssdp_message_s *, BOOL, BOOL, char *, int);
static BOOL parse_url(const char *, char *, int, int *, char *, int);
static void parse_filters(char *, filters_factory_s **, BOOL);
static char *get_ip_address_from_socket(const SOCKET);
static char *get_mac_address_from_socket(const SOCKET, struct sockaddr_storage *, char *);
static BOOL parse_address(const char *, struct sockaddr_storage **, BOOL);
static int findInterface(struct sockaddr_storage *, const char *);
static SOCKET setupSocket(BOOL, BOOL, BOOL, char *, struct sockaddr_storage *, const char *, int, BOOL, BOOL);
static BOOL isMulticastAddress(const char *);
static void init_ssdp_message(ssdp_message_s *);
static BOOL create_plain_text_message(char *, int, ssdp_message_s *);
#ifdef DEBUG___
static int chr_count(char *, char);
static void print_debug(FILE *, const char *, const char*, int, char *, ...);
static void add_alloc(...);
static void remove_alloc(...);
static void print_alloc();
#endif



static void free_stuff() {

  if(notif_server_sock != 0) {
    close(notif_server_sock);
    REMOVE_ALLOC("notif_server_sock");
    notif_server_sock = 0;
  }

  if(notif_client_sock != 0) {
    close(notif_client_sock);
    REMOVE_ALLOC("notid_client_sock");
    notif_client_sock = 0;
  }

  if(notif_recipient_sock != 0) {
    close(notif_recipient_sock);
    REMOVE_ALLOC("notif_recipient_sock");
    notif_recipient_sock = 0;
  }

  if(recv_string != NULL) {
    free(recv_string);
    REMOVE_ALLOC("recv_string");
    recv_string = NULL;
  }

  if(send_string != NULL) {
    free(send_string);
    REMOVE_ALLOC("send_string");
    send_string = NULL;
  }

  if(notif_string != NULL) {
    free(notif_string);
    REMOVE_ALLOC("notif_string");
    notif_string = NULL;
  }

  if(filters_factory != NULL) {
    if(filters_factory->filters != NULL) {
      int fc;
      for(fc = 0; fc < (filters_factory->filters_count); fc++) {
        if(filters_factory->filters[fc].header != NULL) {
          free(filters_factory->filters[fc].header);
          REMOVE_ALLOC("filters_factory->filters[%d].header", fc);
          filters_factory->filters[fc].header = NULL;
        }
        if((filters_factory->filters + fc)->value) {
          free(filters_factory->filters[fc].value);
          REMOVE_ALLOC("filters_factory->filters[%d].value", fc);
          filters_factory->filters[fc].value = NULL;
        }
      }
      free(filters_factory->filters);
      REMOVE_ALLOC("filters_factory->filters");
      filters_factory->filters = NULL;
    }
    free(filters_factory);
    REMOVE_ALLOC("filters_factory");
    filters_factory = NULL;
  }

  if(time_string != NULL) {
    free(time_string);
    REMOVE_ALLOC("time_string");
    time_string = NULL;
  }

  if(notif_recipient_addr != NULL) {
    free(notif_recipient_addr);
    REMOVE_ALLOC("notif_recipient_addr");
    notif_recipient_addr = NULL;
  }

  if(!conf->quiet_mode) {
    printf("\nCleaning up and exitting...\n");
  }

  if(conf != NULL) {
    if(conf->interface != NULL) {
    free(conf->interface);
    REMOVE_ALLOC("conf->interface");
    }
    if(conf->filter != NULL) {
      free(conf->filter);
      REMOVE_ALLOC("conf->filter");
    }
    free(conf);
    REMOVE_ALLOC("conf");
    conf = NULL;
  }
}

static void usage() {
  printf("[A]bused is [B]anks [U]ber-[S]exy-[E]dition [D]aemon\"\n\n");
  printf("USAGE: abused [OPTIONS]\n");
  printf("OPTIONS:\n");
  printf("\t-C <file.conf>    Configuration file to use\n");
  printf("\t-i                Interface to use, default is all\n");
  printf("\t-t                TTL value (routers to hop), default is 1\n");
  printf("\t-s                Show scanned interfaces at start\n");
  printf("\t-l                Include link-local addresses\n");
  printf("\t-b                Print only Bonjour friendly name, not full url\n");
  printf("\t-c                Do not use color when printing\n");
  printf("\t-r                Use red color when printing (default color is green)\n");
  printf("\t-o                Print each devices information in one line\n");
  printf("\t-f <string>       Filter for capturing, 'grep'-like effect. Also works\n");
  printf("\t                  for -u and -U where you can specify a list of\n");
  printf("                    comma separated filters\n");
  printf("\t-S                Run as a server,\n");
  printf("\t                  listens on port 43210 and returns a\n");
  printf("\t                  bonjour scan result (formatted list) for AXIS devices\n");
  printf("\t                  upon receiving the string 'abused'\n");
  printf("\t-d                Run as a UNIX daemon,\n");
  printf("\t                  only works in combination with -S\n");
  printf("\t-u                Listen for local UPnP (SSDP) notifications\n");
  printf("\t-U                Perform an active search for UPnP devices\n");
  printf("\t-a <ip>:<port>    Forward the events to the specified ip and port,\n");
  printf("\t                  also works in combination with -u.\n");
  printf("\t-R                Output unparsed raw data\n");
  printf("\t-F                Do not try to parse the \"Location\" header to get more device info\n");
  printf("\t-x                Convert results to XML before use or output\n");
  printf("\t-4                Force the use of the IPv4 protocol\n");
  printf("\t-6                Force the use of the IPv6 protocol\n");
  printf("\t-q                Be quiet!\n");
  printf("\t-T                The time to wait for a device to answer a query\n");
  printf("\t-L                Enable multicast loopback traffic\n");
}


int main(int argc, char **argv) {
  int opt;
  //struct sockaddr_storage notif_server_addr;
  struct sockaddr_storage notif_client_addr;
  //struct in_addr mcast_address;
  ssdp_message_s ssdp_message;
  int recvLen = 1;
  //struct in_addr mcast_interface;

  //mcast_interface.s_addr = htonl(INADDR_ANY);

  conf = (configuration_s *)malloc(sizeof(configuration_s));
  /* Default configuration */
  conf->interface =             NULL;
  conf->run_as_daemon =         FALSE;
  conf->run_as_server =         FALSE;
  conf->listen_for_upnp_notif = FALSE;
  conf->scan_for_upnp_devices = FALSE;
  conf->forward_enabled =       FALSE;
  conf->raw_output =            FALSE;
  conf->gather =                FALSE;
  conf->gather_silent =         FALSE;
  conf->fetch_info =            TRUE;
  conf->xml_output =            FALSE;
  conf->ttl =                   1;
  conf->show_interfaces =       FALSE;
  conf->hide_link_locals =      TRUE;
  conf->print_in_color =        TRUE;
  conf->print_red =             FALSE;
  conf->bonjour_only =          FALSE;
  conf->one_line =              FALSE;
  conf->filter =                NULL;
  conf->use_ipv4 =              FALSE;
  conf->use_ipv6 =              FALSE;
  conf->quiet_mode =            FALSE;
  conf->upnp_timeout =          MULTICAST_TIMEOUT;
  conf->enable_loopback =       FALSE;

  signal(SIGTERM, &exitSig);
  signal(SIGABRT, &exitSig);
  signal(SIGINT, &exitSig);

  #ifdef DEBUG___
  printf("%sDebug color%s\n", DEBUG_COLOR_BEGIN, DEBUG_COLOR_END);
  printf("%sError color%s\n", ERROR_COLOR_BEGIN, DEBUG_COLOR_END);
  #endif

  /* parse arguments */
  while ((opt = getopt(argc, argv, "i:t:slcrbof:SduUmgGa:RFx64qT:L")) > 0) {
    char *pend = NULL;

    switch (opt) {

    case 'C':
      /*if(!parse_configuration(optarg)) {
        printf("Parsing of '%s' failed!, optarg);
      };*/
      break;

    case 'i':
      conf->interface = optarg;
      break;

    case 't':
      pend = NULL;
      conf->ttl = (unsigned char)strtol(optarg, &pend, 10);
      break;

    case 's':
      conf->show_interfaces = TRUE;
      break;

    case 'l':
      conf->hide_link_locals = FALSE;
      break;

    case 'c':
      conf->print_in_color = FALSE;
      break;

    case 'r':
      conf->print_red = TRUE;
      break;

    case 'b':
      conf->bonjour_only = TRUE;
      break;

    case 'o':
      conf->one_line = TRUE;
      break;

    case 'f':
      conf->filter = optarg;
      break;

    case 'S':
      conf->run_as_server = TRUE;
      break;

    case 'd':
      conf->run_as_daemon = TRUE;
      break;

    case 'u':
      conf->listen_for_upnp_notif = TRUE;
      break;

    case 'U':
      conf->scan_for_upnp_devices = TRUE;
      break;

    case 'a':
    if(optarg != NULL && strlen(optarg) > 0) {
      PRINT_DEBUG("parse_address()");
      if(!parse_address(optarg, &notif_recipient_addr, TRUE)) {
        usage();
        free_stuff();
        exit(EXIT_FAILURE);
      }
      conf->forward_enabled = TRUE;
    }
    break;

    case 'm':
      printf("         {\n      {   }\n       }_{ __{\n    .-{   }   }-.\n   (   }     {   )\n   |`-.._____..-'|\n   |             ;--.\n   |            (__  \\\n   |             | )  )\n   |   ABUSED    |/  /\n   |             /  /\n   |            (  /\n   \\             y'\n    `-.._____..-'\n");
      free_stuff();
      exit(EXIT_SUCCESS);

    case 'g':
      conf->gather = TRUE;
      break;

    case 'G':
      conf->gather_silent = TRUE;
      break;

    case 'R':
      conf->raw_output = TRUE;
      break;

    case 'F':
      conf->fetch_info = FALSE;
      break;

    case 'x':
      conf->xml_output = TRUE;
      break;

    case '4':
      if(conf->use_ipv6) {
        PRINT_ERROR("Cannot use both mutualy exclusive arguments -4 and -6\n");
        free_stuff();
        exit(EXIT_FAILURE);
      }
      conf->use_ipv4 = TRUE;
      break;

    case '6':
      conf->use_ipv6 = TRUE;
      break;

    case 'q':
      conf->quiet_mode = TRUE;
      break;

    case 'T':
      // TODO: errorhandling
      pend = NULL;
      conf->upnp_timeout = (int)strtol(optarg, &pend, 10);
      break;

    case 'L':
      conf->enable_loopback = TRUE;
      break;

    default:
      usage();
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }

  /* If missconfigured, stop and notify the user */
  if(conf->run_as_daemon && !(conf->run_as_server || conf->listen_for_upnp_notif || conf->scan_for_upnp_devices)) {

    PRINT_ERROR("Cannot start as daemon.\nUse -d in combination with -S, -a or with both.\n");
    exit(EXIT_FAILURE);

  }
  /* If run as a daemon */
  else if(conf->run_as_daemon) {

    int fd, max_open_fds;

    /**
    * Daemon preparation steps:
    *
    * 1. Set up rules for ignoring all (tty related)
    *    signals that can stop the process
    *
    * 2. Fork to child and exit parent to free
    *    the terminal and the starting process
    *
    * 3. Change PGID to ignore parent stop signals
    *    and disassociate from controlling terminal
    *    (two solutions, normal and BSD)
    *
    */

    /* If started with init then no need to detach and do all the stuff */
    if(getppid() == 1) {

      goto started_with_init;

    }

    #ifdef SIGTTOU
    signal(SIGTTOU, SIG_IGN);
    #endif

    #ifdef SIGTTIN
    signal(SIGTTIN, SIG_IGN);
    #endif

    #ifdef SIGTSTP
    signal(SIGTSTP, SIG_IGN);
    #endif

    /*  Get new PGID (not PG-leader and not zero) and free parent process
    so terminal can be used/closed */
    if(fork() != 0) {

      /* Exit parent */
      exit(EXIT_SUCCESS);

    }

    #ifdef BSD
    setpgrp();

    if((fd = open("/dev/tty", O_RDWR)) >= 0) {

      /* Get rid of the controlling terminal */
      ioctl(fd, TIOCNOTTY, 0);
      close(fd);

    }
    else {

      PRINT_ERROR("Could not dissassociate from controlling terminal: openning /dev/tty failed.");
      free_stuff();
      exit(EXIT_FAILURE);

    }
    #else
    /* Non-BSD UNIX systems do the above with a single call to setpgrp() */
    setpgrp();

    /* Ignore death if parent dies */
    signal(SIGHUP,SIG_IGN);

    /*  Get new PGID (not PG-leader and not zero)
    because non-BSD systems don't allow assigning
    controlling terminals to non-PG-leader processes */
    if(fork() != 0) {

      /* Exit the parent */
      exit(EXIT_SUCCESS);

    }
    #endif

    started_with_init:

    /* Close all open descriptors */
    max_open_fds = sysconf(_SC_OPEN_MAX);
    for(fd = 0; fd < max_open_fds; fd++) {

      close(fd);

    }

    /* Change cwd in case it was on a mounted system */
    chdir("/");

    /* Clear filemode creation mask */
    umask(0);

  }

  /* If set to listen for AXIS devices notifications then
  fork() and live a separate life */
  if(conf->listen_for_upnp_notif && (conf->run_as_daemon || conf->run_as_server)) {
    if(fork() != 0) {
      /* listen_for_upnp_notif went to the forked process,
       so it set to false in parent so it doesn't run twice'*/
      conf->listen_for_upnp_notif = FALSE;
    }
    else {
      PRINT_DEBUG("Created a process for listening of UPnP notifications");
      /* scan_for_upnp_devices is set to false
       so it doesn't run in this child (if it will be run at all) */
      conf->scan_for_upnp_devices = FALSE;
    }
  }

  /* If set to scan for AXIS devices then
  fork() and live a separate life */
  if(conf->scan_for_upnp_devices && (conf->run_as_daemon || conf->run_as_server)) {
    if(fork() != 0) {
      /* scan_for_upnp_devices went to the forked process,
       so it set to false in parent so it doesn't run twice */
      conf->scan_for_upnp_devices = FALSE;
    }
    else {
      PRINT_DEBUG("Created a process for scanning UPnP devices");
      /* listen_for_upnp_notif is set to false
       so it doesn't run in this child (if it will be run at all) */
      conf->listen_for_upnp_notif = FALSE;
    }
  }

  /* If set to listen for AXIS devices notifications then
  start listening for notifications but never continue
  to do any other work the parent should be doing */
  if(conf->listen_for_upnp_notif) {
    PRINT_DEBUG("listen_for_upnp_notif start");

    /* Init fork allocations */
    notif_string = (char *)malloc(sizeof(char) * NOTIF_RECV_BUFFER);

    /* init socket */
    PRINT_DEBUG("setupSocket()");
    notif_server_sock = setupSocket(FALSE,      // BOOL is_ipv6
              TRUE,       // BOOL is_udp
              TRUE,       // BOOL is_multicast
              conf->interface,  // char *interface
              NULL,       // struct sockaddr_storage *sa
              SSDP_ADDR,  // const char *ip
              SSDP_PORT,  // int port
              TRUE,       // BOOL is_server
              FALSE);     // BOOL keepalive

    /* Parse the filters */
    PRINT_DEBUG("parse_filters()");
    parse_filters(conf->filter, &filters_factory, TRUE);

    /* Child process server loop */
    PRINT_DEBUG("Strating infinite loop");
    BOOL drop_message;
    while(TRUE) {
      drop_message = FALSE;

      memset(notif_string, '\0', NOTIF_RECV_BUFFER);
      #ifndef DEBUG_MSG___
      int size = sizeof(notif_client_addr);
      PRINT_DEBUG("loop: ready to receive");
      recvLen = recvfrom(notif_server_sock, notif_string, NOTIF_RECV_BUFFER, 0,
                        (struct sockaddr *) &notif_client_addr, (socklen_t *)&size);
      if(recvLen < 0) {
        free_stuff();
        PRINT_ERROR("recvfrom(): Failed to receive any data");
        exit(EXIT_FAILURE);
      }
      #else
      // DEV/TEST STUFF, DELETE AFTERWARDS
      sleep(DEBUG_MSG_FREQ___);
      PRINT_DEBUG("using fake UPnP messages");
      strcpy(notif_string, "NOTIFY * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n");
      strcat(notif_string, "CACHE-CONTROL: max-age=1800\r\nLOCATION: ");
      strcat(notif_string, DEBUG_MSG_LOCATION_HEADER);
      strcat(notif_string, "\r\nOPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n01-NLS: 1966d9e6-1dd2-11b2-aa65-a2d9092ea049\r\n");
      strcat(notif_string, "NT: urn:axis-com:service:BasicService:1\r\nNTS: ssdp:alive\r\n");
      strcat(notif_string, "SERVER: Linux/3.4.0, UPnP/1.0, Portable SDK for UPnP devices/1.6.18\r\nX-User-Agent: redsonic\r\n");
      strcat(notif_string, "USN: uuid:Upnp-BasicDevice-1_0-00408C184D0E::urn:axis-com:service:BasicService:1\r\n\r\n");

      PRINT_DEBUG("Setting IP for fake sender");
      notif_client_addr.ss_family = AF_INET;
      if(inet_pton(AF_INET, "10.0.0.2", (void *)&((struct sockaddr_in *)&notif_client_addr)->sin_addr) < 1) {
        PRINT_ERROR("Error setting IP for fake sender (%d)", errno);
      }
      #endif

      /* init ssdp_message */
      init_ssdp_message(&ssdp_message);

      /* Retrieve IP address from socket */
      char *tmp_ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
      // note that tmp_ip should not be freed since it is passed to ssdp_message_s
      memset(tmp_ip, '\0', IPv6_STR_MAX_SIZE);
      if(!inet_ntop(notif_client_addr.ss_family, (notif_client_addr.ss_family == AF_INET ? (void *)&((struct sockaddr_in *)&notif_client_addr)->sin_addr : (void *)&((struct sockaddr_in6 *)&notif_client_addr)->sin6_addr), tmp_ip, IPv6_STR_MAX_SIZE)) {
        PRINT_ERROR("Erroneous IP from sender");
        free_ssdp_message(&ssdp_message);
        free(tmp_ip);
        continue;
      }

      /* Retrieve MAC address from socket (if possible, else NULL) */
      char *tmp_mac = NULL;
      tmp_mac = get_mac_address_from_socket(notif_server_sock, (struct sockaddr_storage *)&notif_client_addr, NULL);

      /* Build the ssdp message struct */
      BOOL build_success = build_ssdp_message(&ssdp_message, tmp_ip, tmp_mac, recvLen, notif_string);

      if(!build_success) {
        free_ssdp_message(&ssdp_message);
        continue;
      }

      // TODO: Make it recognize both AND and OR (search for ; inside a ,)!!!
      // TODO: add "request" string filtering

      /* Check if notification should be used (to print and possibly send to the given destination) */
      ssdp_header_s *ssdp_headers = ssdp_message.headers;
      if(filters_factory != NULL) {
      PRINT_DEBUG("traversing filters");
      int fc;
      for(fc = 0; fc < filters_factory->filters_count; fc++) {
        if(strcmp(filters_factory->filters[fc].header, "ip") == 0 && strstr(ssdp_message.ip, filters_factory->filters[fc].value) == NULL) {
          drop_message = TRUE;
          break;
        }

        while(ssdp_headers) {
          if(strcmp(get_header_string(ssdp_headers->type, ssdp_headers), filters_factory->filters[fc].header) == 0 && strstr(ssdp_headers->contents, filters_factory->filters[fc].value) == NULL) {
            drop_message = TRUE;
            break;
          }
          ssdp_headers = ssdp_headers->next;
          }
          ssdp_headers = ssdp_message.headers;

          if(drop_message) {
            break;
          }
        }
      }

      if(filters_factory == NULL || !drop_message) {

        char *results = (char *)malloc(sizeof(char) * XML_BUFFER_SIZE);

        /* If unable to  allocate, report it but don't break
           in case other non-results actions exist */
        if(!results) {
          PRINT_ERROR("Could not allocate buffer memory");
        }
        memset(results, '\0', sizeof(char) * XML_BUFFER_SIZE);

        /* Handle the messages */
        if(conf->gather) {
          PRINT_DEBUG("Gathering mode is not supported yet!");
        }
        else if(conf->gather_silent) {
          PRINT_DEBUG("Silent gathering mode is not supported yet!");
        }
        else if(results && conf->raw_output) {
          snprintf(results, strlen(notif_string), "\n\n%s\n\n", notif_string);
        }
        else if(results && conf->xml_output) {
          to_xml(&ssdp_message, conf->fetch_info, FALSE, results, XML_BUFFER_SIZE);
        }
        else if(results && !create_plain_text_message(results, XML_BUFFER_SIZE, &ssdp_message)) {
            PRINT_ERROR("Failed creating plain-text message");
            free(results);
            results = NULL;
        }

        /* Check if forwarding ('-a') is enabled */
        if(results && conf->forward_enabled) {
          PRINT_DEBUG("setupSocket() for forwarding");
          // TODO: Make it not send M*SEARCH uPnP packets
          send_stuff("/abused/post.php", results, notif_recipient_addr, 80, 1);
        }
        /* Else just display results on console */
        else if(results) {
          printf("\n\n%s", results);
        }
        else {
          PRINT_DEBUG("results is NULL");
        }

        if(results) {
          PRINT_DEBUG("freeing results");
          free(results);
          results = NULL;
        }

      }

      PRINT_DEBUG("loop done");

      free_ssdp_message(&ssdp_message);
      // TODO: remove me
      break;
    }

  /* We can never get this far */
  } // listen_for_upnp_notif end

  /* If set to scan for AXIS devices then
  start scanning but never continue
  to do any other work the parent should be doing */
  if(conf->scan_for_upnp_devices) {
    PRINT_DEBUG("scan_for_upnp_devices begin");

    /* init sending socket */
    PRINT_DEBUG("setupSocket() request");
    notif_client_sock = setupSocket(conf->use_ipv6,  // BOOL is_ipv6
              TRUE,       // BOOL is_udp
              TRUE,       // BOOL is_multicast
              conf->interface,  // char *interface
              NULL,       // struct sockaddr_storage *sa
              (conf->use_ipv6 ? SSDP_ADDR6_LL : SSDP_ADDR),  // const char *ip
              SSDP_PORT,  // int port
              FALSE,      // BOOL is_server
              FALSE);     // BOOL keepalive

    /* Parse the filters */
    PRINT_DEBUG("parse_filters()");
    parse_filters(conf->filter, &filters_factory, TRUE);

    char *response = (char *)malloc(sizeof(char) * NOTIF_RECV_BUFFER);
    char *request = (char *)malloc(sizeof(char) * 2048);
    memset(request, '\0', sizeof(char) * 2048);
    strcpy(request, "M-SEARCH * HTTP/1.1\r\n");
    strcat(request, "Host:239.255.255.250:1900\r\n");
    //strcat(request, "ST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n");
    strcat(request, "ST:urn:axis-com:service:BasicService:1\r\n");
    //strcat(request, "ST:urn:schemas-upnp-org:device:SwiftServer:1\r\n");
    strcat(request, "Man:\"ssdp:discover\"\r\n");
    strcat(request, "MX:3\r\n\r\n");

    BOOL drop_message;

    /* Client (mcast group) address */
    PRINT_DEBUG("creating multicast address");
    struct addrinfo *addri;
    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = (conf->use_ipv6 ? AF_INET6 : AF_INET);
    hint.ai_socktype = SOCK_DGRAM;
    hint.ai_protocol = IPPROTO_UDP;
    hint.ai_canonname = NULL;
    hint.ai_addr = NULL;
    hint.ai_next = NULL;
    char *port_str = (char *)malloc(sizeof(char) * 6);
    memset(port_str, '\0', 6);
    sprintf(port_str, "%d", SSDP_PORT);
    int err = 0;
    if((err = getaddrinfo((conf->use_ipv6 ? SSDP_ADDR6_LL : SSDP_ADDR), port_str, &hint, &addri)) != 0) {
      PRINT_ERROR("getaddrinfo(): %s\n", gai_strerror(err));
      free(port_str);
      free(response);
      free(request);
      free_stuff();
      exit(EXIT_FAILURE);
    }
    free(port_str);

    /* Send the UPnP request */
    PRINT_DEBUG("sending request");
    /*****************/
    // This is the temp solution untill the
    // commented-out sendto() code below is fixed
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(SSDP_PORT);
    inet_pton(AF_INET, SSDP_ADDR, &sa.sin_addr);
    /*****************/
    //recvLen = sendto(notif_client_sock, request, strlen(request), 0, (struct sockaddr *)&addri->ai_addr, addri->ai_addrlen);
    recvLen = sendto(notif_client_sock, request, strlen(request), 0, (struct sockaddr *)&sa, sizeof(sa));
    size_t sento_addr_len = sizeof(struct sockaddr_storage);
    struct sockaddr_storage *sendto_addr = (struct sockaddr_storage *)malloc(sento_addr_len);
    memset(sendto_addr, 0, sento_addr_len);
    int response_port = 0;
    if(getsockname(notif_client_sock, (struct sockaddr *)sendto_addr, (socklen_t *)&sento_addr_len) < 0) {
      PRINT_ERROR("Could not get sendto() port, going to miss the replies");
    }
    #ifdef DEBUG___
    else {
      response_port = ntohs(sendto_addr->ss_family == AF_INET ? ((struct sockaddr_in *)sendto_addr)->sin_port : ((struct sockaddr_in6 *)sendto_addr)->sin6_port);
      PRINT_DEBUG("sendto() port is %d", response_port);
    }
    #endif
    close(notif_client_sock);
    PRINT_DEBUG("sent %d bytes", recvLen);
    free(request);
    freeaddrinfo(addri);
    if(recvLen < 0) {
      free_stuff();
      PRINT_ERROR("sendto(): Failed sending any data");
      exit(EXIT_FAILURE);
    }

    drop_message = FALSE;

    size_t size = sizeof(notif_client_addr);

    /* init listening socket */
    PRINT_DEBUG("setupSocket() listening");
    notif_server_sock = setupSocket(conf->use_ipv6,  // BOOL is_ipv6
              TRUE,       // BOOL is_udp
              FALSE,       // BOOL is_multicast
              conf->interface,  // char *interface
              NULL,       // struct sockaddr_storage *sa
              NULL,       // const char *ip
              response_port,  // int port
              TRUE,       // BOOL is_server
              FALSE);     // BOOL keepalive

    /* Set socket timeout, to timeout 2 seconds after devices stop answering */
    struct timeval rtimeout;
    rtimeout.tv_sec = conf->upnp_timeout;
    rtimeout.tv_usec = 0;

    PRINT_DEBUG("setting socket receive-timeout to %d", (int)rtimeout.tv_sec);
    if(setsockopt(notif_server_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&rtimeout, sizeof(rtimeout)) < 0) {
      PRINT_ERROR("setsockopt() SO_RCVTIMEO: (%d) %s", errno, strerror(errno));
      free(response);
      free_stuff();
      exit(EXIT_FAILURE);
    }
    PRINT_DEBUG("reciving responses");
    do {
      /* Wait for an answer */
      memset(response, '\0', NOTIF_RECV_BUFFER);
      recvLen = recvfrom(notif_server_sock, response, NOTIF_RECV_BUFFER, 0,
               (struct sockaddr *) &notif_client_addr, (socklen_t *)&size);
      PRINT_DEBUG("Recived %d bytes%s", (recvLen < 0 ? 0 : recvLen), (recvLen < 0 ? " (timed out)" : ""));

      if(recvLen < 0) {
        continue;
      }

      /* init ssdp_message */
      init_ssdp_message(&ssdp_message);

      /* Extract IP */
      char *tmp_ip = (char*)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
      memset(tmp_ip, '\0', IPv6_STR_MAX_SIZE);
      if(!inet_ntop(notif_client_addr.ss_family, (notif_client_addr.ss_family == AF_INET ? (void *)&((struct sockaddr_in *)&notif_client_addr)->sin_addr : (void *)&((struct sockaddr_in6 *)&notif_client_addr)->sin6_addr), tmp_ip, IPv6_STR_MAX_SIZE)) {
        PRINT_ERROR("inet_ntop(): (%d) %s", errno, strerror(errno));
        free_ssdp_message(&ssdp_message);
        free_stuff();
        exit(EXIT_FAILURE);
      }

      char *tmp_mac = (char *)malloc(sizeof(char) * MAC_STR_MAX_SIZE);
      memset(tmp_mac, '\0', MAC_STR_MAX_SIZE);
      tmp_mac = get_mac_address_from_socket(notif_server_sock, &notif_client_addr, NULL);

      BOOL build_success = build_ssdp_message(&ssdp_message, tmp_ip, tmp_mac, recvLen, response);
      if(!build_success) {
        if(conf->raw_output) {
          printf("\n\n\n%s\n", response);
        }
        continue;
      }

      // TODO: Make it recognize both AND and OR (search for ; inside a ,)!!!
      // TODO: add "request" string filtering
      /* Check if notification should be used (to print and possibly send to the given destination) */
      ssdp_header_s *ssdp_headers = ssdp_message.headers;
      if(filters_factory != NULL) {
        int fc;
        for(fc = 0; fc < filters_factory->filters_count; fc++) {
          if(strcmp(filters_factory->filters[fc].header, "ip") == 0 && strstr(ssdp_message.ip, filters_factory->filters[fc].value) == NULL) {
            drop_message = TRUE;
            break;
          }

          while(ssdp_headers) {
            if(strcmp(get_header_string(ssdp_headers->type, ssdp_headers), filters_factory->filters[fc].header) == 0 && strstr(ssdp_headers->contents, filters_factory->filters[fc].value) == NULL) {
              drop_message = TRUE;
              break;
            }
            ssdp_headers = ssdp_headers->next;
          }
          ssdp_headers = ssdp_message.headers;

          if(drop_message) {
            break;;
          }
        }
      }

      if(filters_factory == NULL || !drop_message) {

        /* Print the message */
        if(conf->gather) {
          PRINT_DEBUG("Gathering mode is not coded yet!\n");
        }
        else if(conf->gather_silent) {
          PRINT_DEBUG("Silent gathering mode is not coded yet!\n");
        }
        else if(conf->raw_output) {
          printf("\n\n\n%s\n", response);
        }
        else if(conf->xml_output) {
          char *xml_string = (char *)malloc(sizeof(char) * XML_BUFFER_SIZE);
          to_xml(&ssdp_message, conf->fetch_info, FALSE, xml_string, XML_BUFFER_SIZE);
          printf("%s\n", xml_string);
          free(xml_string);
        }
        else {
          printf("\n\n\n----------BEGIN NOTIFICATION------------\n");
          printf("Time received: %s\n", ssdp_message.datetime);
          printf("Origin-MAC: %s\n", (ssdp_message.mac != NULL ? ssdp_message.mac : "(Could not be determined)"));
          printf("Origin-IP: %s\nMessage length: %d Bytes\n", ssdp_message.ip, ssdp_message.message_length);
          printf("Request: %s\nProtocol: %s\n", ssdp_message.request, ssdp_message.protocol);
          if(conf->fetch_info) {
            int bytes_fetched = 0;
            char *fetched_string = (char *)malloc(sizeof(char) * XML_BUFFER_SIZE);
            memset(fetched_string, '\0', XML_BUFFER_SIZE);
            bytes_fetched = fetch_upnp_device_info(&ssdp_message, fetched_string, XML_BUFFER_SIZE);
            if(bytes_fetched > 0) {
              printf("%s", fetched_string);
            }
            free(fetched_string);
          }

          int hc = 0;

          while(ssdp_headers) {
            printf("Header[%d][type:%d;%s]: %s\n", hc, ssdp_headers->type, get_header_string(ssdp_headers->type, ssdp_headers), ssdp_headers->contents);
            ssdp_headers = ssdp_headers->next;
            hc++;
          }
          ssdp_headers = NULL;
          printf("-----------END NOTIFICATION-------------\n");
        }
        /* TODO: Send the message back to -a */

      }

      free_ssdp_message(&ssdp_message);
    } while(recvLen > 0);
    free(response);
    free_stuff();
    PRINT_DEBUG("scan_for_upnp_devices end");
    exit(EXIT_SUCCESS);
  } // scan_for_upnp_devices end

  if(!conf->listen_for_upnp_notif) {
    usage();
  }

  free_stuff();
  exit(EXIT_SUCCESS);
} // main end

static void exitSig(int param) {
  free_stuff();
  exit(EXIT_SUCCESS);
}

/**
* Returns the appropriate unsigned char (number) representation of the header string
*
* @param const char *header_string The header string to be looked up
*
* @return unsigned char A unsigned char representing the header type
*/
static unsigned char get_header_type(const char *header_string) {
  int headers_size;
  int header_string_length = 0;
  char *header_lower = NULL;
  const char *header_strings[] = {
    SSDP_HEADER_UNKNOWN_STR,
    SSDP_HEADER_HOST_STR,
    SSDP_HEADER_ST_STR,
    SSDP_HEADER_MAN_STR,
    SSDP_HEADER_MX_STR,
    SSDP_HEADER_CACHE_STR,
    SSDP_HEADER_LOCATION_STR,
    SSDP_HEADER_HOST_STR,
    SSDP_HEADER_OPT_STR,
    SSDP_HEADER_01NLS_STR,
    SSDP_HEADER_NT_STR,
    SSDP_HEADER_NTS_STR,
    SSDP_HEADER_SERVER_STR,
    SSDP_HEADER_XUSERAGENT_STR,
    SSDP_HEADER_USN_STR
  };
  headers_size = sizeof(header_strings)/sizeof(char *);

  header_string_length = strlen(header_string);
  if(header_string_length < 1) {
    PRINT_ERROR("Erroneous header string detected");
    return (unsigned char)SSDP_HEADER_UNKNOWN;
  }
  header_lower = (char *)malloc(sizeof(char) * (header_string_length + 1));

  memset(header_lower, '\0', sizeof(char) * (header_string_length + 1));

  int i;
  for(i = 0; header_string[i] != '\0'; i++){
    header_lower[i] = tolower(header_string[i]);
  }

  for(i=0; i<headers_size; i++) {
    if(strcmp(header_lower, header_strings[i]) == 0) {
      free(header_lower);
      return (unsigned char)i;
    }
  }
  free(header_lower);
  return (unsigned char)SSDP_HEADER_UNKNOWN;
}

/**
* Returns the appropriate string representation of the header type
*
* @param const unsigned int header_type The header type (int) to be looked up
*
* @return char * A string representing the header type
*/
static const char *get_header_string(const unsigned int header_type, const ssdp_header_s *header) {
  const char *header_strings[] = {
    SSDP_HEADER_UNKNOWN_STR,
    SSDP_HEADER_HOST_STR,
    SSDP_HEADER_ST_STR,
    SSDP_HEADER_MAN_STR,
    SSDP_HEADER_MX_STR,
    SSDP_HEADER_CACHE_STR,
    SSDP_HEADER_LOCATION_STR,
    SSDP_HEADER_HOST_STR,
    SSDP_HEADER_OPT_STR,
    SSDP_HEADER_01NLS_STR,
    SSDP_HEADER_NT_STR,
    SSDP_HEADER_NTS_STR,
    SSDP_HEADER_SERVER_STR,
    SSDP_HEADER_XUSERAGENT_STR,
    SSDP_HEADER_USN_STR
  };

  if((header_type == 0 || header_type > (sizeof(header_strings)/sizeof(char *) - 1)) && header != NULL && header->unknown_type != NULL) {
    return header->unknown_type;
  }

  return header_strings[header_type];
}

/**
* Parse a single SSDP header
*
* @param ssdp_header_s *header The location where the parsed result should be stored
* @param const char *raw_header The header string to be parsed
*/
static void build_ssdp_header(ssdp_header_s *header, const char *raw_header) {
/*
[172.26.150.15][458B] "NOTIFY * HTTP/1.1
HOST: 239.255.255.250:1900
CACHE-CONTROL: max-age=1800
LOCATION: http://172.26.150.15:49154/rootdesc1.xml
OPT: "http://schemas.upnp.org/upnp/1/0/"; ns=01
01-NLS: 1966d9e6-1dd2-11b2-aa65-a2d9092ea049
NT: urn:axis-com:service:BasicService:1
NTS: ssdp:alive
SERVER: Linux/3.4.0, UPnP/1.0, Portable SDK for UPnP devices/1.6.18
X-User-Agent: redsonic
USN: uuid:Upnp-BasicDevice-1_0-00408C184D0E::urn:axis-com:service:BasicService:1

"
*/
  int header_name_length = -1, header_contents_length = -1;
  int raw_header_length = 0;
  char *header_name;

  header_name_length = strpos(raw_header, ":");
  if(header_name_length < 1) {
    return;
  }
  header_name = (char *)malloc(sizeof(char) * (header_name_length + 1));
  memset(header_name, '\0', sizeof(char) * (header_name_length + 1));
  strncpy(header_name, raw_header, header_name_length);

  header->type = (unsigned char)get_header_type(header_name);
  if(header->type == SSDP_HEADER_UNKNOWN) {
    header->unknown_type = (char *)malloc(sizeof(char) * header_name_length + 1);
    memset(header->unknown_type, '\0', header_name_length + 1);
    strcpy(header->unknown_type, header_name);
  }
  else {
    header->unknown_type = NULL;
  }

  raw_header_length = strlen(raw_header);
  header_contents_length = raw_header_length - header_name_length + 2;
  header->contents = (char *)malloc(sizeof(char) * header_contents_length);
  memset(header->contents, '\0', sizeof(char) * header_contents_length);
  strcpy(header->contents, &raw_header[header_name_length + 1 + (raw_header[header_name_length + 2] == ' ' ? 1 : 0)]); // avoid ": " in header contents
  free(header_name);
}

/**
* Parse a SSDP message
*
* @param ssdp_message_s *message The location where the parsed result should be stored
* @param const char *ip The IP address of the sender
* @param const char *mac The MAC address of the sender
* @param const const int *message_length The message length
* @param const char *raw_message The message string to be parsed
*/
static BOOL build_ssdp_message(ssdp_message_s *message, char *ip, char *mac, int message_length, const char *raw_message) {
  char *raw_header;
  int newline = 0, last_newline = 0;
  int raw_message_left = strlen(raw_message);
  time_t t;

  t = time(NULL);
  strftime(message->datetime, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));

  message->mac = mac;
  message->ip = ip;
  message->message_length = message_length;

  /* find end of request string */
  last_newline = strpos(raw_message, "\r\n");
  if(last_newline < 0) {
    free(message->datetime);
    PRINT_DEBUG("build_ssdp_message() failed: last_newline < 0");
    return FALSE;
  }
  PRINT_DEBUG("last_newline: %d", last_newline);
  /* get past request string, point at first header row */
  last_newline += 2;

  /* save request string and protocol */
  /* TODO: make it search and save
  * http-answer if present too
  * eg. in "HTTP/1.1 200 OK",
  * see if next is not newline and space
  * and then save all the way to newline.
  */
  newline = strpos(raw_message, "HTTP");
  if(newline < 0) {
    free(message->datetime);
    PRINT_DEBUG("build_ssdp_message() failed: newline < 0");
    return FALSE;
  }
  strncpy(message->request, raw_message, (!newline? newline : newline - 1));
  strncpy(message->protocol, &raw_message[newline], last_newline - 2 - newline);

  /* allocate starting header heap */
  message->headers = (ssdp_header_s *)malloc(sizeof(ssdp_header_s));
  message->headers->first = message->headers;

  BOOL has_next_header = TRUE;

  do {
    int pos;

    /* find where new header ends in raw message */
    pos = strpos(&raw_message[last_newline], "\r\n");
    if(pos < 0) {
      free(message->datetime);
      PRINT_DEBUG("build_ssdp_message() failed: pos < 0");
      return FALSE;
    }
    newline = last_newline + pos;
    /* allocate new raw header (header name, contents and '\0') heap */
    raw_header = (char *)malloc(sizeof(char) * (newline - last_newline + 1));
    /* fill it with row from raw_message */
    memset(raw_header, '\0', (newline - last_newline + 1));
    strncpy(raw_header, &raw_message[last_newline], newline - last_newline);

    /* now lets go and build the header struct */
    build_ssdp_header(message->headers, raw_header);
    if(message->headers->contents) {
      message->header_count++;
    }

    free(raw_header);

    raw_message_left = strlen(&raw_message[newline]);
    if(raw_message_left < 4 || (raw_message[newline] == '\r' && raw_message[newline + 1] == '\n' &&
      raw_message[newline + 2] == '\r' && raw_message[newline + 3] == '\n')) {
      has_next_header = FALSE;
    }

    /* if there is another header */
    if(has_next_header) {
      /* make newline last_newline and skip "\r\n"*/
      last_newline = newline + 2;
      /* if this header was successfully created */
      if(message->headers->contents) {
        /* allocate next header heap */
        message->headers->next = (ssdp_header_s *)malloc(sizeof(ssdp_header_s));
        memset(message->headers->next, '\0', sizeof(ssdp_header_s));
        /* pass over first header pointer */
        message->headers->next->first = message->headers->first;
        /* and move to it */
        message->headers = message->headers->next;
      }
    }
    else {
      message->headers->next = NULL;
    }

  } while(has_next_header);

  /* reset chain (linked list) */
  message->headers = message->headers->first;

  return TRUE;

}

/**
* Fins the position of a string in a string (or char in a string)
*
* @return int The position of the string/char in the searched string
*/
static int strpos(const char *haystack, const char *needle) {
  char *p = strstr(haystack, needle);
  if(p) {
    return p-haystack;
  }
  return -1;
}

/**
* Counts how many times a string is present in another string
*
* @param const char * haystack The string to be searched in
* @param const char * needle The string to be searched for
*
* @return unsigned char The number of times needle is preent in haystack
*/
static unsigned char strcount(const char *haystack, const char *needle) {
  unsigned char count = 0;
  const char *pos = haystack;

  do {
    pos = strstr(pos, needle);
    if(pos != NULL) {
      count++;
      pos++;
    }
  } while(pos != NULL);

  return count;

}

// TODO: remove port, it is contained in **pp_address
static int send_stuff(const char *url, const char *data, const struct sockaddr_storage *da, int port, int timeout) {

  if(url == NULL || strlen(url) < 1) {
    PRINT_ERROR("send_stuff(): url not set");
    return FALSE;
  }

  /* Create socket */
  PRINT_DEBUG("send_stuff(): creating socket");
  SOCKET send_sock = setupSocket(conf->use_ipv6,
            FALSE,
            FALSE,
            conf->interface,
            NULL,
            NULL,
            0,
            FALSE,
            FALSE);

  if(send_sock == SOCKET_ERROR) {
    PRINT_ERROR("send_stuff(): %d, %s", errno, strerror(errno));
    return 0;
  }

  struct timeval stimeout, rtimeout;
  stimeout.tv_sec = 1;
  stimeout.tv_usec = 0;
  rtimeout.tv_sec = timeout;
  rtimeout.tv_usec = 0;

  PRINT_DEBUG("send_stuff(): setting socket receive-timeout to %d", (int)rtimeout.tv_sec);
  if (setsockopt(send_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&rtimeout, sizeof(rtimeout)) < 0) {
    PRINT_ERROR("send_stuff(): setsockopt() SO_RCVTIMEO: %d, %s", errno, strerror(errno));
    close(send_sock);
    return 0;
  }

  PRINT_DEBUG("setting socket send-timeout to %d", (int)stimeout.tv_sec);
  if(setsockopt (send_sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&stimeout, sizeof(stimeout)) < 0) {
    PRINT_ERROR("send_stuff(); setsockopt() SO_SNDTIMEO, %d, %s", errno, strerror(errno));
    close(send_sock);
    return 0;
  }

  /* Setup socket destination address */
  PRINT_DEBUG("send_stuff(): setting up socket addresses");

  if(da->ss_family == AF_INET) {
    struct sockaddr_in *da_ipv4 = (struct sockaddr_in *)da;
    da_ipv4->sin_port = htons(port);
  }
  else {
    struct sockaddr_in6 *da_ipv6 = (struct sockaddr_in6 *)da;
    da_ipv6->sin6_port = htons(port);
  }

  /* Connect socket to destination */
  #ifdef DEBUG___
  char *tmp_ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(tmp_ip, '\0', IPv6_STR_MAX_SIZE);
  inet_ntop(da->ss_family, (da->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)da)->sin_addr : (void *)&((struct sockaddr_in6 *)da)->sin6_addr), tmp_ip, IPv6_STR_MAX_SIZE);
  PRINT_DEBUG("send_stuff(): connecting to destination (%s; ss_family = %s [%d])", tmp_ip, (da->ss_family == AF_INET ? "AF_INET" : "AF_INET6"), da->ss_family);
  free(tmp_ip);
  tmp_ip = NULL;
  #endif
  if(connect(send_sock, (struct sockaddr*)da, sizeof(struct sockaddr)) == SOCKET_ERROR) {
    PRINT_ERROR("send_stuff(): connect(): %d, %s", errno, strerror(errno));
    close(send_sock);
    return 0;
  }

  /*
  POST </path/file.html> HTTP/1.0\r\n
  Host: <own_ip>\r\n
  User-Agent: abused-<X>\r\n
  \r\n
  */
  char *ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(ip, '\0', IPv6_STR_MAX_SIZE);
  inet_ntop(da->ss_family, (da->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)da)->sin_addr : (void *)&((struct sockaddr_in6 *)da)->sin6_addr), ip, IPv6_STR_MAX_SIZE);

  int request_size = 5096;
  char *request = (char *)malloc(sizeof(char) * request_size);
  memset(request, '\0', request_size);

  int used_length = 0;
  used_length = snprintf(request + used_length,
           request_size - used_length,
           "POST %s HTTP/1.1\r\n", url);
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "Host: %s\r\n", ip);
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "Connection: close\r\n");
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "User-Agent: abused-%s\r\n", ABUSED_VERSION);
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "Content-type: text/xml\r\n");
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "Content-length: %d\r\n\r\n", (int)strlen(data));
  used_length += snprintf(request + used_length,
            request_size - used_length,
            "%s\r\n\r\n", data);
  free(ip);

  PRINT_DEBUG("send_stuff(): sending string:\n%s", request);
  int bytes = send(send_sock, request, strlen(request), 0);
  free(request);
  if(bytes < 1) {
    PRINT_ERROR("Failed forwarding message");
    close(send_sock);
    return 0;
  }

  PRINT_DEBUG("send_stuff(): sent %d bytes", bytes);
  int response_size = 10240; // 10KB
  char *response = (char *)malloc(sizeof(char) * response_size);
  memset(response, '\0', response_size);

  int all_bytes = 0;
  do {
    bytes = recv(send_sock, response + all_bytes, response_size - all_bytes, 0);
    all_bytes += bytes;
  } while(bytes > 0);

  PRINT_DEBUG("send_stuff(): received %d bytes", all_bytes);
  PRINT_DEBUG("send_stuff(): received:\n%s", response);

  free(response);
  PRINT_DEBUG("send_stuff(): closing socket");
  close(send_sock);
  return all_bytes;
}

/**
* Frees all neccessary allocations in a ssdp_message_s
*
* @param ssdp_message_s *message The message to free allocations for
*/
static void free_ssdp_message(ssdp_message_s *message) {
  ssdp_header_s *next_header = NULL;

  if(!message) {
  return;
  }
  if(message->mac != NULL) {
    free(message->mac);
    message->mac = NULL;
  }
  if(message->ip != NULL) {
    free(message->ip);
    message->ip = NULL;
  }
  if(message->request != NULL) {
    free(message->request);
    message->request = NULL;
  }
  if(message->protocol != NULL) {
    free(message->protocol);
    message->protocol = NULL;
  }
  if(message->answer != NULL) {
    free(message->answer);
    message->answer = NULL;
  }
  if(message->datetime != NULL) {
    free(message->datetime);
    message->datetime = NULL;
  }

  do {
    if(message->headers->contents != NULL) {
      free(message->headers->contents);
      message->headers->contents = NULL;
    }
    if(message->headers->unknown_type != NULL) {
      free(message->headers->unknown_type);
      message->headers->unknown_type = NULL;
    }
    next_header = message->headers->next;
    free(message->headers);
    message->headers = next_header;
    next_header = NULL;
  } while(message->headers);
}

/**
* Parses the filter argument
*
* @param const char *raw_filter The raw filter string
* @param const unsigned char filters_count The number of filters found
* @param const char **filters The parsed filters array
*/
static void parse_filters(char *raw_filter, filters_factory_s **filters_factory, BOOL print_filters) {
  char *pos = NULL, *last_pos = raw_filter, *splitter = NULL;
  unsigned char filters_count = 0;
  filters_factory_s *ff = NULL;

  if(raw_filter == NULL || strlen(raw_filter) < 1) {
    if(print_filters) {
      printf("No filters applied.\n");
    }
    return;
  }

  /* Get rid of leading and trailing ',' and '=' */
  while(*last_pos == ',' || *last_pos == '=') {
    last_pos++;
  }

  while(*(last_pos + strlen(last_pos) - 1) == ',' || *(last_pos + strlen(last_pos) - 1) == '=') {
    memcpy((last_pos + strlen(last_pos) - 1), "\0", 1);
  }

  /* Number of filters (filter delimiters) found */
  filters_count = strcount(last_pos, ",") + 1;

  /* Create filter factory (a container for global use) */
  *filters_factory = (filters_factory_s *)malloc(sizeof(filters_factory_s));

  /* Simplify access name and initialize */
  ff = *filters_factory;
  memset(ff, '\0', sizeof(filters_factory_s));
  ff->filters_count = filters_count;

  /* Create place for all the filters */
  ff->filters = (filter_s *)malloc(sizeof(filter_s) * filters_count);
  memset(ff->filters, '\0', sizeof(filter_s) * filters_count);

  int fc;
  for(fc = 0; fc < filters_count; fc++) {

    /* Header name which value to apply filter on */
    ff->filters[fc].header = (char *)malloc(sizeof(char) * 64);
    memset((ff->filters[fc]).header, '\0', sizeof(char) * 64);

    /* Header value to filter on */
    ff->filters[fc].value = (char *)malloc(sizeof(char) * 2048);
    memset(ff->filters[fc].value, '\0', sizeof(char) * 2048);

    /* Find filter splitter (',') */
    pos = strstr(last_pos, ",");
    if(pos == NULL) {
      pos = last_pos + strlen(last_pos);
    }

    /* Find name and value splitter ('=') */
    splitter = strstr(last_pos, "=");
    if(splitter > 0) {
      strncpy(ff->filters[fc].header, last_pos, splitter - last_pos);
      splitter++;
      strncpy(ff->filters[fc].value, splitter, pos - splitter);
    }
    else {
      strncpy(ff->filters[fc].header, last_pos, pos - last_pos);
    }
    last_pos = pos + 1;

  }

  if(print_filters) {
  printf("\nFilters applied:\n");
  int c;
    for(c = 0; c < filters_count; c++) {
      printf("%d: %s = %s\n", c, ff->filters[c].header, ff->filters[c].value);
    }
  }
}

/**
* Get the remote IP address from a given sock
*
* @param sock The socket to extract the IP address from
*
* @return char The IP address as a string
*/
static char *get_ip_address_from_socket(const SOCKET sock) {
// maybe needs a PF_INET type of socket?
  struct ifreq ifr[10];
  struct ifconf ifc;
  memset(ifr, 0, sizeof(ifr));
  ifc.ifc_len = sizeof(ifr);
  ifc.ifc_req = ifr;
  char *result = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);

  if(ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
  PRINT_ERROR("get_ip_address_from_socket(); ioctl() SIOCGIFCONF: (%d) %s", errno, strerror(errno));
  free(result);
  free_stuff();
  exit(EXIT_FAILURE);
  }

  int i, ifc_length = ifc.ifc_len / sizeof(struct ifreq);
  for(i = 0; i < ifc_length; i++) {
    memset(result, '\0', IPv6_STR_MAX_SIZE);
    if((conf->use_ipv4 && ifr[i].ifr_addr.sa_family == AF_INET) ||
     (conf->use_ipv6 && ifr[i].ifr_addr.sa_family == AF_INET6) ||
     (!conf->use_ipv4 && !conf->use_ipv6)) {
      if (ioctl(sock, SIOCGIFADDR, &ifr[i]) == 0) {
        int sa_fam = ifr[i].ifr_addr.sa_family;
        if(sa_fam == AF_INET && inet_ntop(sa_fam, &((struct sockaddr_in *)(&ifr[i].ifr_addr))->sin_addr.s_addr, result, IPv4_STR_MAX_SIZE)) {
          if(strcmp(result, "127.0.0.1") == 0) {
            continue;
          }
        }
        else if(sa_fam == AF_INET6 && inet_ntop(sa_fam, &((struct sockaddr_in6 *)(&ifr[i].ifr_addr))->sin6_addr.s6_addr, result, IPv6_STR_MAX_SIZE)) {
          // nothing to skip that I am aware of
        }
        else {
          PRINT_ERROR("get_ip_address_from_socket(); inet_ntop(): (%d) %s", errno, strerror(errno));
          free(result);
          free_stuff();
          exit(EXIT_FAILURE);
        }
        PRINT_DEBUG("found IP form socket: %s", result);
        return result;
      }
      PRINT_ERROR("get_ip_address_from_socket(); ioctl() SIOCGIFADDR: (%d) %s", errno, strerror(errno));
    }
  }
  free(result);
  return NULL;
}


/**
* Get the remote MAC address from a given sock
*
* @param sock The socket to extract the MAC address from
*
* @return int The remote MAC address as a string
*/
// TODO: fix for IPv6
static char *get_mac_address_from_socket(const SOCKET sock, struct sockaddr_storage *sa_ip, char *ip) {
  char *mac_string = (char *)malloc(sizeof(char) * MAC_STR_MAX_SIZE);
  memset(mac_string, '\0', MAC_STR_MAX_SIZE);

  #if defined BSD || defined APPLE
  /* xxxBSD or MacOS solution */
  PRINT_DEBUG("Using BSD style MAC discovery (sysctl)");
  int sysctl_flags[] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO };
  char *arp_buffer = NULL;
  char *arp_buffer_end = NULL;
  char *arp_buffer_next = NULL;
  struct rt_msghdr *rtm = NULL;
  struct sockaddr_inarp *sin_arp = NULL;
  struct sockaddr_dl *sdl = NULL;
  size_t arp_buffer_size;
  BOOL found_arp = FALSE;
  struct sockaddr_storage *tmp_sin = NULL;

  /* Sanity check, choose IP source */
  if(NULL == sa_ip && NULL != ip) {
    tmp_sin = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
    memset(tmp_sin, '\0', sizeof(struct sockaddr_storage));
    if(!inet_pton(sa_ip->ss_family, ip, (sa_ip->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)sa_ip)->sin_addr : (void *)&((struct sockaddr_in6 *)sa_ip)->sin6_addr))) {
      PRINT_ERROR("get_mac_address_from_socket(): Failed to get MAC from given IP (%s)", ip);
      free(mac_string);
      free(tmp_sin);
      return NULL;
    }
    sa_ip = tmp_sin;
  }
  else if(NULL == sa_ip) {
    PRINT_ERROR("Neither IP nor SOCKADDR given, MAC not fetched");
    free(mac_string);
    return NULL;
  }

  /* See how much we need to allocate */
  if(sysctl(sysctl_flags, 6, NULL, &arp_buffer_size, NULL, 0) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): sysctl(): Could not determine neede size");
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Allocate needed memmory */
  if((arp_buffer = malloc(arp_buffer_size)) == NULL) {
    PRINT_ERROR("get_mac_address_from_socket(): Failed to allocate memory for the arp table buffer");
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Fetch the arp table */
  if(sysctl(sysctl_flags, 6, arp_buffer, &arp_buffer_size, NULL, 0) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): sysctl(): Could not retrieve arp table");
    free(arp_buffer);
    free(mac_string);
    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
    return NULL;
  }

  /* Loop through the arp table/list */
  arp_buffer_end = arp_buffer + arp_buffer_size;
  for(arp_buffer_next = arp_buffer; arp_buffer_next < arp_buffer_end; arp_buffer_next += rtm->rtm_msglen) {

    /* See it through another perspective */
    rtm = (struct rt_msghdr *)arp_buffer_next;

    /* Skip to the address portion */
    sin_arp = (struct sockaddr_inarp *)(rtm + 1);
    sdl = (struct sockaddr_dl *)(sin_arp + 1);

    /* Check if address is the one we are looking for */
    if(((struct sockaddr_in *)sa_ip)->sin_addr.s_addr != sin_arp->sin_addr.s_addr) {
      continue;
    }
    found_arp = TRUE;

    /* The proudly save the MAC to a string */
    if (sdl->sdl_alen) {
      unsigned char *cp = (unsigned char *)LLADDR(sdl);
      sprintf(mac_string, "%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
    }

    free(arp_buffer);

    if(NULL != tmp_sin) {
      free(tmp_sin);
    }
  }
  #else
  /* Linux (and rest) solution */
  PRINT_DEBUG("Using Linux style MAC discovery (ioctl SIOCGARP)");
  struct arpreq arp;
  unsigned char *mac = NULL;

  memset(&arp, '\0', sizeof(struct arpreq));
  sa_family_t ss_fam = AF_INET;
  struct in_addr *sin_a = NULL;
  struct in6_addr *sin6_a = NULL;
  BOOL is_ipv6 = FALSE;

  /* Sanity check */
  if(sa_ip == NULL && ip == NULL){
    PRINT_ERROR("Neither IP string nor sockaddr given, MAC not fetched");
    free(mac_string);
    return NULL;
  }

  /* Check if IPv6 */
  struct sockaddr_in6 *tmp = (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6));
  if((sa_ip != NULL && sa_ip->ss_family == AF_INET6)
  || (ip != NULL && inet_pton(AF_INET6, ip, tmp))) {
    is_ipv6 = TRUE;
    ss_fam = AF_INET6;
    PRINT_DEBUG("Looking for IPv6-MAC association");
  }
  else if((sa_ip != NULL && sa_ip->ss_family == AF_INET)
  || (ip != NULL && inet_pton(AF_INET, ip, tmp))) {
    PRINT_DEBUG("Looking for IPv4-MAC association");
  }
  else {
    PRINT_ERROR("sa_ip or ip variable error");
  }
  free(tmp);
  tmp = NULL;

  /* Assign address family for arpreq */
  ((struct sockaddr_storage *)&arp.arp_pa)->ss_family = ss_fam;

  /* Point the needed pointer */
  if(!is_ipv6){
    sin_a = &((struct sockaddr_in *)&arp.arp_pa)->sin_addr;
  }
  else if(is_ipv6){
    sin6_a = &((struct sockaddr_in6 *)&arp.arp_pa)->sin6_addr;
  }

  /* Fill the fields */
  if(sa_ip != NULL) {
    if(ss_fam == AF_INET) {
      *sin_a = ((struct sockaddr_in *)sa_ip)->sin_addr;
    }
    else {
      *sin6_a = ((struct sockaddr_in6 *)sa_ip)->sin6_addr;
    }
  }
  else if(ip != NULL) {

    if(!inet_pton(ss_fam,
        ip,
        (ss_fam == AF_INET ? (void *)sin_a: (void *)sin6_a))) {
    PRINT_ERROR("Failed to get MAC from given IP (%s)", ip);
    PRINT_ERROR("Error %d: %s", errno, strerror(errno));
    free(mac_string);
    return NULL;
    }
  }

  /* Go through all interfaces' arp-tables and search for the MAC */
  /* Possible explanation: Linux arp-tables are if-name specific */
  struct ifaddrs *interfaces = NULL, *ifa = NULL;
  if(getifaddrs(&interfaces) < 0) {
    PRINT_ERROR("get_mac_address_from_socket(): getifaddrs():Could not retrieve interfaces");
    free(mac_string);
    return NULL;
  }

  /* Start looping through the interfaces*/
  for(ifa = interfaces; ifa && ifa->ifa_next; ifa = ifa->ifa_next) {

    /* Skip the loopback interface */
    if(strcmp(ifa->ifa_name, "lo") == 0) {
      PRINT_DEBUG("Skipping interface 'lo'");
      continue;
    }

    /* Copy current interface name into the arpreq structure */
    PRINT_DEBUG("Trying interface '%s'", ifa->ifa_name);
    strncpy(arp.arp_dev, ifa->ifa_name, 15);

    ((struct sockaddr_storage *)&arp.arp_ha)->ss_family = ARPHRD_ETHER;

    /* Ask for thE arp-table */
    if(ioctl(sock, SIOCGARP, &arp) < 0) {

      /* Handle failure */
      if(errno == 6) {
        /* if error is "Unknown device or address" then continue/try next interface */
        PRINT_DEBUG("get_mac_address_from_socket(): ioctl(): (%d) %s", errno, strerror(errno));
        continue;
      }
      else {
        PRINT_ERROR("get_mac_address_from_socket(): ioctl(): (%d) %s", errno, strerror(errno));
        continue;
      }
    }

    mac = (unsigned char *)&arp.arp_ha.sa_data[0];

  }

  if(!mac) {
    PRINT_DEBUG("mac is NULL");
    free(mac_string);
    return NULL;
  }

  PRINT_DEBUG("sprintf()");
  sprintf(mac_string, "%x:%x:%x:%x:%x:%x", *mac, *(mac + 1), *(mac + 2), *(mac + 3), *(mac + 4), *(mac + 5));
  mac = NULL;
  #endif

  PRINT_DEBUG("Determined MAC string: %s", mac_string);
  return mac_string;
}

/**
* Fetches additional info from a UPnP message "Location" header
*
* @param ssdp_message_s * The message whos "Location" header to use
*
* @return char * A string containing the fetched information
*/
static int fetch_upnp_device_info(const ssdp_message_s *ssdp_message, char *info, int info_size) {
  int bytes_written = 0;
  char *location_header = NULL;
  ssdp_header_s *ssdp_headers = ssdp_message->headers;
  while(ssdp_headers) {
    if(ssdp_headers->type == SSDP_HEADER_LOCATION) {
      location_header = ssdp_headers->contents;
      // Ex. location_header:
      //http://10.83.128.46:2869/upnphost/udhisapi.dll?content=uuid:59e293c8-9179-4efb-ac32-3c9514238505
      PRINT_DEBUG("found header_location: %s", location_header);
    }
    ssdp_headers = ssdp_headers->next;
  }
  ssdp_headers = NULL;

  if(location_header != NULL) {

    /* URL parsing allocations*/
    PRINT_DEBUG("allocating for URL parsing");
    char *ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
    int port = 0;
    char *rest = (char *)malloc(sizeof(char) * 256);
    char *request = (char *)malloc(sizeof(char) * 1024); // 1KB
    char *response = (char *)malloc(sizeof(char) * DEVICE_INFO_SIZE); // 8KB
    memset(ip, '\0', IPv6_STR_MAX_SIZE);
    memset(rest, '\0', 256);
    memset(request, '\0', 1024);
    memset(response, '\0', DEVICE_INFO_SIZE);

    /* Try to parse the location_header URL */
    PRINT_DEBUG("trying to parse URL");
    if(parse_url(location_header, ip, IPv6_STR_MAX_SIZE, &port, rest, 256)) {

      /* Create socket */
      PRINT_DEBUG("creating socket");
      SOCKET resolve_sock = setupSocket(conf->use_ipv6,
                 FALSE,
                 FALSE,
                 conf->interface,
                 NULL,
                 NULL,
                 0,
                 FALSE,
                 FALSE);

      if(resolve_sock == SOCKET_ERROR) {
        PRINT_ERROR("fetch_upnp_device_info(); setupSocket(): (%d) %s", errno, strerror(errno));
        free(ip);
        free(rest);
        free(request);
        free(response);
        return 0;
      }

      struct timeval stimeout, rtimeout;
      stimeout.tv_sec = 1;
      stimeout.tv_usec = 0;
      rtimeout.tv_sec = 5;
      rtimeout.tv_usec = 0;

      PRINT_DEBUG("setting socket receive-timeout to %d", (int)rtimeout.tv_sec);
      if (setsockopt (resolve_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&rtimeout, sizeof(rtimeout)) < 0) {
        PRINT_ERROR("fetch_upnp_device_info(); setsockopt() SO_RCVTIMEO: (%d) %s", errno, strerror(errno));
        free(ip);
        free(rest);
        free(request);
        free(response);
        return 0;
      }

      PRINT_DEBUG("setting socket send-timeout to %d", (int)stimeout.tv_sec);
      if(setsockopt (resolve_sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&stimeout, sizeof(stimeout)) < 0) {
        PRINT_ERROR("fetch_upnp_device_info(); setsockopt() SO_SNDTIMEO: (%d) %s", errno, strerror(errno));
        free(ip);
        free(rest);
        free(request);
        free(response);
        return 0;
      }

      /* Setup socket destination address */
      PRINT_DEBUG("setting up socket addresses");
      struct sockaddr_storage *da = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
      memset(da, 0, sizeof(struct sockaddr_storage));
      da->ss_family = conf->use_ipv6 ? AF_INET6 : AF_INET;
      if(!inet_pton(da->ss_family, ip, (da->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)da)->sin_addr : (void *)&((struct sockaddr_in6 *)da)->sin6_addr))) {
        int ip_length = strlen(ip);
        if(ip_length < 1) {
          PRINT_ERROR("The destination IP address could be determined (%s)\n", (ip_length < 1 ? "empty" : ip));
        }
        free(ip);
        free(rest);
        free(request);
        free(response);
        free(da);
        return 0;
      }

      if(da->ss_family == AF_INET) {
        struct sockaddr_in *da_ipv4 = (struct sockaddr_in *)da;
        da_ipv4->sin_port = htons(port);
      }
      else {
        struct sockaddr_in6 *da_ipv6 = (struct sockaddr_in6 *)da;
        da_ipv6->sin6_port = htons(port);
      }

      /* Connect socket to destination */
      #ifdef DEBUG___
      char *tmp_ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
      memset(tmp_ip, '\0', IPv6_STR_MAX_SIZE);
      inet_ntop(da->ss_family, (da->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)da)->sin_addr : (void *)&((struct sockaddr_in6 *)da)->sin6_addr), tmp_ip, IPv6_STR_MAX_SIZE);
      PRINT_DEBUG("connecting to destination (%s; ss_family = %s [%d])", tmp_ip, (da->ss_family == AF_INET ? "AF_INET" : "AF_INET6"), da->ss_family);
      free(tmp_ip);
      tmp_ip = NULL;
      #endif
      if(connect(resolve_sock, (struct sockaddr*)da, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        PRINT_ERROR("fetch_upnp_device_info(); connect(): (%d) %s", errno, strerror(errno));
        free(ip);
        free(rest);
        free(request);
        free(response);
        free(da);
        return 0;
      }

      free(da);
      int used_length = 0;
      /*
      GET </path/file.html> HTTP/1.0\r\n
      Host: <own_ip>\r\n
      User-Agent: abused-<X>\r\n
      \r\n
      */
      used_length += snprintf(request + used_length, 1024 - used_length, "GET %s HTTP/1.0\r\n", rest);
      used_length += snprintf(request + used_length, 1024 - used_length, "Host: %s\r\n", ip);
      snprintf(request + used_length, 1024 - used_length, "User-Agent: abused-%s\r\n\r\n", ABUSED_VERSION);
      PRINT_DEBUG("sending string:\n%s", request);
      int bytes = send(resolve_sock, request, strlen(request), 0);
      PRINT_DEBUG("sent %d bytes", bytes);
      int all_bytes = 0;
      do {
        bytes = 0;
        bytes = recv(resolve_sock, response + all_bytes, DEVICE_INFO_SIZE - all_bytes, 0);
        all_bytes += bytes;
      } while(bytes > 0);
      PRINT_DEBUG("received %d bytes", all_bytes);
      PRINT_DEBUG("closing socket");
      close(resolve_sock);

      /* Extract the usefull info */
      char *friendlyName = (char *)malloc(sizeof(char) * 128);
      char *manufacturer = (char *)malloc(sizeof(char) * 128);
      char *manufacturerURL = (char *)malloc(sizeof(char) * 128);
      char *modelName = (char *)malloc(sizeof(char) * 128);
      char *modelNumber = (char *)malloc(sizeof(char) * 128);
      char *modelURL = (char *)malloc(sizeof(char) * 128);
      char *tmp_pointer = NULL;
      memset(friendlyName, '\0', 128);
      memset(manufacturer, '\0', 128);
      memset(manufacturerURL, '\0', 128);
      memset(modelName, '\0', 128);
      memset(modelNumber, '\0', 128);
      memset(modelURL, '\0', 128);

      /* Newline-separated info */
      tmp_pointer = strstr(response, "<friendlyName>");
      if(tmp_pointer) {
        strncpy(friendlyName, tmp_pointer + 14, (int)(strstr(response, "</friendlyName>") - (tmp_pointer + 14)));
        bytes_written += snprintf(info + bytes_written, info_size - bytes_written, "Friendly name: %s\n", friendlyName);
      }
      tmp_pointer = strstr(response, "<manufacturer>");
      if(tmp_pointer) {
        strncpy(manufacturer, tmp_pointer + 14, (int)(strstr(response, "</manufacturer>") - (tmp_pointer + 14)));
        bytes_written += snprintf(info + bytes_written, info_size - bytes_written, "Manufacturer: %s\n", manufacturer);
      }
      tmp_pointer = strstr(response, "<manufacturerURL>");
      if(tmp_pointer) {
        strncpy(manufacturerURL, tmp_pointer + 17, (int)(strstr(response, "</manufacturerURL>") - (tmp_pointer + 17)));
        bytes_written += snprintf(info + bytes_written, info_size - bytes_written, "Manufacturer URL: %s\n", manufacturerURL);
      }
      tmp_pointer = strstr(response, "<modelName>");
      if(tmp_pointer) {
        strncpy(modelName, tmp_pointer + 11, (int)(strstr(response, "</modelName>") - (tmp_pointer + 11)));
        bytes_written += snprintf(info + bytes_written, info_size - bytes_written, "Model name: %s\n", modelName);
      }
      tmp_pointer = strstr(response, "<modelNumber>");
      if(tmp_pointer) {
        strncpy(modelNumber, tmp_pointer + 13, (int)(strstr(response, "</modelNumber>") - (tmp_pointer + 13)));
        bytes_written += snprintf(info + bytes_written, info_size - bytes_written, "Model number: %s\n", modelNumber);
      }
      tmp_pointer = strstr(response, "<modelURL>");
      if(tmp_pointer) {
        strncpy(modelURL, tmp_pointer + 10, (int)(strstr(response, "</modelURL>") - (tmp_pointer + 10)));
        bytes_written += snprintf(info + bytes_written, info_size - bytes_written, "Model URL: %s\n", modelURL);
      }

      /* I AM FREEEEE!!!! */
      free(friendlyName);
      free(manufacturer);
      free(manufacturerURL);
      free(modelName);
      free(modelNumber);
      free(modelURL);
    }

    free(ip);
    free(rest);
    free(request);
    free(response);
  }

  return bytes_written;
}

/**
* Converts a UPnP message to a XML string
*
* @param ssdp_message_s * The message to be converted
*
* @return char * A string containing the message in XML format
*/
static void to_xml(const ssdp_message_s *ssdp_message, BOOL fetch_info, BOOL hide_headers, char *xml_message, int xml_size) {
  int usedLength = 0;

  if(xml_message == NULL) {
  PRINT_ERROR("to_xml(): No XML message buffer specified");
  }
  else if(ssdp_message == NULL) {
  PRINT_ERROR("to_xml(): No SSDP message specified");
  }
  else if(xml_size < XML_BUFFER_SIZE) {
  PRINT_ERROR("to_xml(): XML buffer is too small (given %d, min allowed is %d)", xml_size, XML_BUFFER_SIZE);
  }

  memset(xml_message, '\0', sizeof(char) * xml_size);
  usedLength = snprintf(xml_message, xml_size,
  "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<root>\n");
  usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
  "\t<message length=\"%d\">\n", ssdp_message->message_length);
  usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
  "\t\t<mac>\n\t\t\t%s\n\t\t</mac>\n", ssdp_message->mac);
  usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
  "\t\t<ip>\n\t\t\t%s\n\t\t</ip>\n", ssdp_message->ip);
  usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
  "\t\t<request protocol=\"%s\">\n\t\t\t%s\n\t\t</request>\n", ssdp_message->protocol, ssdp_message->request);
  usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
  "\t\t<datetime>\n\t\t\t%s\n\t\t</datetime>\n", ssdp_message->datetime);

  if(fetch_info) {
    char *fetched_string = (char *)malloc(sizeof(char) * 2048);
    const char *tags[] = {
      "friendlyName",
      "manufacturer",
      "manufacturerURL",
      "modelName",
      "modelNumber",
      "modelURL"
    };
    const char *fields[] = {
      "Friendly name",
      "Manufacturer",
      "Manufacturer URL",
      "Model name",
      "Model number",
      "Model URL"
    };

    /* Calculate needed buffer size */
    int fields_count = sizeof(fields) / sizeof(char *);
    int fields_size = 0;
    int tags_count = sizeof(tags) / sizeof(char *);
    int tags_size = 0;
    int i;
    for(i = 0; i < fields_count; i++) {
      fields_size += strlen(fields[i]) + 1; // + ": " - "\n" = 2 - 1 = 1 char
    }
    for(i = 0; i < tags_count; i++) {
      tags_size += strlen(tags[i]) + 50; // + the xml formatting below = 18
    }

    memset(fetched_string, '\0', 2048);
    int fetched_length = fetch_upnp_device_info(ssdp_message, fetched_string, 2048);

    /* Check if buffer has enought room */
    #ifdef DEBUG___
    int left   = xml_size - usedLength;
    int needed = fetched_length - fields_size + tags_size;
    PRINT_DEBUG("buffer size check: %d < %d = %s (fetched_length = %d)", left, needed, (left < needed ? "TRUE (FAIL)" : "FALSE (OK)"), fetched_length);
    #endif
    if((fetched_length > 0) && (xml_size - usedLength < fetched_length - fields_size + tags_size)) {
      PRINT_ERROR("to_xml(): Not enought memory in the buffer when formatting device info , skipping\n");
    }
    else if(fetched_length > 0) {

      /* Convert to XML format */
      usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
      "\t\t<custom_fields count=\"%d\">\n", fields_count);

      char *tmp_pointer = NULL;
      char *tmp_pointer_end = NULL;
      char *tmp_string = (char *)malloc(sizeof(char) * 1024);
      for(i = 0; i < fields_count; i++) {
        tmp_pointer = strstr(fetched_string, fields[i]);
        memset(tmp_string, '\0', 1024);
        if(tmp_pointer != NULL) {
          tmp_pointer +=  strlen(fields[i]) + 2;
          tmp_pointer_end = strchr(tmp_pointer, '\n');
          strncpy(tmp_string, tmp_pointer, (int)(tmp_pointer_end - tmp_pointer));
          usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
            "\t\t\t<custom_field name=\"%s\">\n\t\t\t\t%s\n\t\t\t</custom_field>\n", tags[i], tmp_string);
        }
      }
      free(tmp_string);
      tmp_string = NULL;

      usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
      "\t\t</custom_fields>\n");
      PRINT_DEBUG("actual bytes: %d", (int)strlen(xml_message));
    }

    free(fetched_string);
    fetched_string = NULL;
  }

  if(!hide_headers) {
    ssdp_header_s *ssdp_headers = ssdp_message->headers;

    usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
    "\t\t<headers count=\"%d\">\n", (unsigned int)ssdp_message->header_count);
    while(ssdp_headers) {
      usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
      "\t\t\t<header typeInt=\"%d\" typeStr=\"%s\">\n", ssdp_headers->type, get_header_string(ssdp_headers->type, ssdp_headers));
      usedLength += snprintf(xml_message + usedLength, xml_size - usedLength,
      "\t\t\t\t%s\n\t\t\t</header>\n", ssdp_headers->contents);
      ssdp_headers = ssdp_headers->next;
    }
    ssdp_headers = NULL;
  }
  snprintf(xml_message + usedLength, xml_size - usedLength,
  "\t\t</headers>\n\t</message>\n</root>\n");
}

// TODO: remove ip_size ?
static BOOL parse_url(const char *url, char *ip, int ip_size, int *port, char *rest, int rest_size) {

  /* Check if HTTPS */
  if(!url || strlen(url) < 8) {
    PRINT_ERROR("The argument 'url' is not set or is empty");
    return FALSE;
  }

  PRINT_DEBUG("passed url: %s", url);

  if(*(url + 4) == 's') {
    /* HTTPS is not supported at the moment, skip */
    PRINT_DEBUG("HTTPS is not supported, skipping");
    return FALSE;
  }

  const char *ip_begin = strchr(url, ':') + 3;              // http[s?]://^<ip>:<port>/<rest>
  PRINT_DEBUG("ip_begin: %s", ip_begin);
  BOOL is_ipv6 = *ip_begin == '[' ? TRUE : FALSE;
  PRINT_DEBUG("is_ipv6: %s", (is_ipv6 ? "TRUE" : "FALSE"));

  char *rest_begin = NULL;
  if(is_ipv6) {
    if(!(rest_begin = strchr(ip_begin, ']')) || !(rest_begin = strchr(rest_begin, '/'))) {
      // [<ipv6>]^:<port>/<rest>             && :<port>^/<rest>
      PRINT_ERROR("Error: (IPv6) rest_begin is NULL\n");
      return FALSE;
    }
    PRINT_DEBUG("rest_begin: %s", rest_begin);
  }
  else {
    if(!(rest_begin = strchr(ip_begin, '/'))) {                // <ip>:<port>^/<rest>
      PRINT_ERROR("Error: rest_begin is NULL\n");
      return FALSE;
    }
  }

  if(rest_begin != NULL) {
    strcpy(rest, rest_begin);
  }

  char *working_str = (char *)malloc(sizeof(char) * 256); // temporary string buffer
  char *ip_with_brackets = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE + 2); // temporary IP buffer
  memset(working_str, '\0', 256);
  memset(ip_with_brackets, '\0', IPv6_STR_MAX_SIZE + 2);
  PRINT_DEBUG("size to copy: %d", (int)(rest_begin - ip_begin));
  strncpy(working_str, ip_begin, (size_t)(rest_begin - ip_begin)); // "<ip>:<port>"
  PRINT_DEBUG("working_str: %s", working_str);
  char *port_begin = strrchr(working_str, ':');                 // "<ip>^:<port>^"
  PRINT_DEBUG("port_begin: %s", port_begin);
  if(port_begin != NULL) {
    *port_begin = ' ';                                           // "<ip>^ <port>^"
    // maybe memset instead of value assignment ?
    sscanf(working_str, "%s %d", ip_with_brackets, port);      // "<%s> <%d>"
    PRINT_DEBUG("port: %d", *port);
    if(strlen(ip_with_brackets) < 1) {
      PRINT_ERROR("Malformed header_location detected in: '%s'", url);
      free(working_str);
      free(ip_with_brackets);
      return FALSE;
    }
    if(*ip_with_brackets == '[') {
      PRINT_DEBUG("ip_with_brackets: %s", ip_with_brackets);
      //strncpy(ip, ip_with_brackets + 1, strlen(ip_with_brackets) - 1); // uncomment in case we dont need brackets
    }
    else {
      //strcpy(ip, ip_with_brackets); // uncomment in case we dont need brackets
    }
      strcpy(ip, ip_with_brackets);
      PRINT_DEBUG("ip: %s", ip);
  }

  if(rest_begin == NULL) {
    PRINT_ERROR("Malformed header_location detected in: '%s'", url);
    free(working_str);
    free(ip_with_brackets);
    return FALSE;
  }

  free(working_str);
  free(ip_with_brackets);
  return TRUE;
}

/**
* Parse a given ip:port combination into an internet address structure
*
* @param char *raw_address The <ip>:<port> string combination to be parsed
* @param sockaddr_storage ** The IP internet address structute to be allocated and filled
*
* @return BOOL TRUE on success
*/
static BOOL parse_address(const char *raw_address, struct sockaddr_storage **pp_address, BOOL print_results) {
  char *ip;
  struct sockaddr_storage *address = NULL;
  int colon_pos = 0;
  int port = 0;
  BOOL is_ipv6;

  if(strlen(raw_address) < 1) {
    PRINT_DEBUG("parse_address(): No valid IP address specified");
    return FALSE;
  }

  /* Allocate the input address */
  *pp_address = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
  /* Get rid of the "pointer to a pointer" */
  address = *pp_address;
  memset(address, 0, sizeof(struct sockaddr_storage));

  colon_pos = strpos(raw_address, ":");
  PRINT_DEBUG("COLON IS: %d", colon_pos);

  if(colon_pos < 1) {
    PRINT_DEBUG("parse_address(): No valid port specified (%s)\n", raw_address);
    return FALSE;
  }

  ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);

  /* Get rid of [] if IPv6 */
  strncpy(ip, strchr(raw_address, '[') + 1, strchr(raw_address, '[') - strchr(raw_address, ']'));

  is_ipv6 = inet_pton(AF_INET6, ip, address);
  PRINT_DEBUG("is_ipv6 = %s", is_ipv6 ? "TRUE" : "FALSE");
  if(is_ipv6) {
    address->ss_family = AF_INET6;
    port = atoi(strrchr(raw_address, ':') + 1);
  }
  else {
    memset(ip, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);
    strncpy(ip, raw_address, colon_pos);
    if(!inet_pton(AF_INET, ip, &(((struct sockaddr_in *)address)->sin_addr))) {
      printf("No valid IP address specified (%s)\n", ip);
      return FALSE;
    }
    address->ss_family = AF_INET;
    port = atoi(raw_address + colon_pos + 1);
  }

  if(port < 80 || port > PORT_MAX_NUMBER) {
    printf("No valid port specified (%d)\n", port);
    return FALSE;
  }

  if(is_ipv6) {
    ((struct sockaddr_in6 *)address)->sin6_port = htons(port);
  }
  else {
    ((struct sockaddr_in *)address)->sin_port = htons(port);
  }

  if(print_results) {
    memset(ip, '\0', sizeof(char) * IPv4_STR_MAX_SIZE);
    inet_ntop(address->ss_family, (address->ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)address)->sin_addr) : (void *)&(((struct sockaddr_in6 *)address)->sin6_addr)), ip, sizeof(char) * IPv6_STR_MAX_SIZE);
    printf("Results forwarding is enabled, ");
    printf("forwarding to IP %s on port %d\n", ip, ntohs((address->ss_family == AF_INET ? ((struct sockaddr_in *)address)->sin_port : ((struct sockaddr_in6 *)address)->sin6_port)));
  }

  free(ip);

  return TRUE;
}

/**
* Tries to find a interface with the specified name or IP address
*
* @param struct sockaddr_storage *saddr A pointer to the buffer of the address that should be updated
* @param char * address The address or interface name we want to find
*
* @return int The interface index
*/
static int findInterface(struct sockaddr_storage *saddr, const char *address) {
  // TODO: for porting to Windows see http://msdn.microsoft.com/en-us/library/aa365915.aspx
  struct ifaddrs *interfaces, *ifa;
  char *paddr = NULL;
  int ifindex = -1;

  if(getifaddrs(&interfaces) < 0) {
    printf("getifaddrs(): could not find any interfaces\n");
    free_stuff();
    exit(EXIT_FAILURE);
  }

  /* If address not set or is bind-on-all */
  if(address == NULL || (strlen(address) == 0) || (strcmp(address, "0.0.0.0") == 0)) {
    if(conf->use_ipv6) {
      if(saddr != NULL) {
        saddr->ss_family = AF_INET6;
        struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)saddr;
        saddr6->sin6_addr = in6addr_any;
      }
      else {
        PRINT_DEBUG("findInterface(): saddr is NULL [conf->use_ipv6 == TRUE]");
      }
      if(!conf->quiet_mode) {
        PRINT_DEBUG("findInterface(): Matched all addresses (::)\n");
      }
    }
    else {
      if(saddr != NULL) {
        saddr->ss_family = AF_INET;
        struct sockaddr_in *saddr4 = (struct sockaddr_in *)saddr;
        saddr4->sin_addr.s_addr = htonl(INADDR_ANY);
      }
      else {
        PRINT_DEBUG("findInterface(): saddr is NULL [conf->use_ipv6 == FALSE]");
      }
      if(!conf->quiet_mode) {
        PRINT_DEBUG("findInterface(): Matched all addresses (0.0.0.0)\n");
      }
    }
    freeifaddrs(interfaces);
    return 0;
  }

  paddr = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);

  for(ifa=interfaces; ifa&&ifa->ifa_next; ifa=ifa->ifa_next) {

    memset(paddr, '\0', sizeof(char) * 46);
    if(!conf->use_ipv6 && ifa->ifa_addr->sa_family == AF_INET) {
      inet_ntop(AF_INET, (void *)&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, paddr, IPv6_STR_MAX_SIZE);
    }
    else if(!conf->use_ipv4 && ifa->ifa_addr->sa_family == AF_INET6) {
      inet_ntop(AF_INET6, (void *)&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr, paddr, IPv6_STR_MAX_SIZE);
    }
    else {
      continue;
    }

    if((strcmp(address, ifa->ifa_name) == 0) || (strcmp(address, paddr) == 0)) {
      ifindex = if_nametoindex(ifa->ifa_name);

      if(!conf->quiet_mode) {
        printf("Matched interface (with index %d) name '%s' with %s address %s\n", ifindex, ifa->ifa_name, (ifa->ifa_addr->sa_family == AF_INET ? "IPv4" : "IPv6"), paddr);
      }

      PRINT_DEBUG("conf->use_ipv4: %s; conf->use_ipv6: %s", (conf->use_ipv4 ? "TRUE" : "FALSE"), (conf->use_ipv6 ? "TRUE" : "FALSE"));

      if(!conf->use_ipv6 && ifa->ifa_addr->sa_family == AF_INET) {
        if(saddr != NULL) {
          struct sockaddr_in *saddr4 = ((struct sockaddr_in *)saddr);
          struct sockaddr_in *ifaddr4 = (struct sockaddr_in *)ifa->ifa_addr;
          saddr4->sin_addr = ifaddr4->sin_addr;
          PRINT_DEBUG("setting IPv4 saddr...");
        }
      }
      else if(!conf->use_ipv4){
        if(saddr != NULL) {
          struct sockaddr_in6 *saddr6 = ((struct sockaddr_in6 *)saddr);
          struct sockaddr_in6 *ifaddr6 = (struct sockaddr_in6 *)ifa->ifa_addr;
          saddr6->sin6_addr = ifaddr6->sin6_addr;
          PRINT_DEBUG("set IPv6 saddr");
        }
      }

      if(saddr != NULL) {
        saddr->ss_family = ifa->ifa_addr->sa_family;
      }

      break;
    }

  }

  if(interfaces) {
    freeifaddrs(interfaces);
  }

  if(paddr != NULL) {
  free(paddr);
  }

  return ifindex;
}

// TODO: Do...
static configuration_s *parseConfigurationFile(const char *file_location) {
  configuration_s *conf = NULL;
  PRINT_DEBUG("parseConfigurationFile() begin");
  /*if(file_location == NULL || file_exists(file_location)) {
  PRINT_ERROR("File '%s' does not exist", file_location);
  free_stuff();
  exit(EXIT_FAILURE);
  }*/
  PRINT_DEBUG("parseConfigurationFile() end");
  return conf;
}

/* Creates a connection to a given host */
// TODO: check that interface is freed!!!!
static SOCKET setupSocket(BOOL is_ipv6, BOOL is_udp, BOOL is_multicast, char *interface, struct sockaddr_storage *sa, const char *ip, int port, BOOL is_server, BOOL keepalive) {
  char *mcast_str = NULL;
  SOCKET sock;
  int sock_type, protocol;
  int ifindex = 0;
  struct ip_mreq mreq;
  struct ipv6_mreq mreq6;
  struct sockaddr_storage *saddr;

  saddr = (struct sockaddr_storage *)malloc(sizeof(struct sockaddr_storage));
  memset(saddr, 0, sizeof(struct sockaddr_storage));
  memset(&mreq, 0, sizeof(mreq));
  memset(&mreq6, 0, sizeof(mreq6));

  if(interface != NULL && sa != NULL) {
    PRINT_ERROR("setupSocket(): Both an interface and a socket address have been passed as arguments. These are mutually exclusive, one must be NULL.\n");
    free(saddr);
    free_stuff();
    exit(EXIT_FAILURE);
  }

  if(sa != NULL) {
    interface = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
    memset(interface, '\0', sizeof(char) * IPv6_STR_MAX_SIZE);
    inet_ntop(sa->ss_family, (sa->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)sa)->sin_addr : (void *)&((struct sockaddr_in6 *)sa)->sin6_addr), interface, IPv6_STR_MAX_SIZE);
  }

  ifindex = findInterface(saddr, interface);
  if(ifindex < 0) {
    PRINT_ERROR("The requested interface '%s' could not be found\n", interface);
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    free_stuff();
    exit(EXIT_FAILURE);
  }

  #ifdef DEBUG___
  {
    char a[100];
    memset(a, '\0', 100);
    if(!inet_ntop(saddr->ss_family, (saddr->ss_family == AF_INET ? (void *)&((struct sockaddr_in *)saddr)->sin_addr : (void *)&((struct sockaddr_in6 *)saddr)->sin6_addr), a, 100)) {
      PRINT_ERROR("setupSocket(); inet_ntop(): (%d) %s", errno, strerror(errno));
    }
    PRINT_DEBUG("findInterface() returned: saddr->sin_addr: %s", a);
  }
  #endif

  if(is_multicast && (ip != NULL && strlen(ip) > 0)) {
    if(!isMulticastAddress(ip)) {
      PRINT_ERROR("The specified IP (%s) is not a multicast address\n", ip);
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }

  if(is_ipv6) {
    saddr->ss_family = AF_INET6;
    protocol = IPPROTO_IPV6;
  }
  else {
    saddr->ss_family = AF_INET;
    protocol = IPPROTO_IP;
  }
  sock_type = is_udp ? SOCK_DGRAM : SOCK_STREAM;

  /* init socket */
  sock = socket(saddr->ss_family, sock_type, protocol);
  if(sock < 0) {
    PRINT_ERROR("Failed to create socket. (%d) %s", errno, strerror(errno));
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    free_stuff();
    exit(EXIT_FAILURE);
  }

  /* Make able to receive from an already used port */
  int reuse = 1;
  if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
    PRINT_ERROR("setsockopt() SO_REUSEADDR: (%d) %s", errno, strerror(errno));
    free(saddr);
    if(sa != NULL) {
      free(interface);
    }
    free_stuff();
    exit(EXIT_FAILURE);
  }
  /* linux >= 3.9
  if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1) {
  PRINT_ERROR("setsockopt() SO_REUSEPORT: (%d) %s", errno, strerror(errno));
  free_stuff();
  exit(EXIT_FAILURE);
  }
  */

  /* Set keepalive */
  if(keepalive) {
    if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepalive, sizeof(char)) < 0) {
      PRINT_ERROR("setsockopt() SO_KEEPALIVE: (%d) %s", errno, strerror(errno));
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }

  /* Set TTL */
  if(is_multicast) {
    if(setsockopt(sock, protocol, (is_ipv6 ? IPV6_MULTICAST_HOPS : IP_MULTICAST_TTL), (char *)&conf->ttl, sizeof(char)) < 0) {
      PRINT_ERROR("setsockopt() SIP(6)_MULTICAST_TTL/HOP: (%d) %s", errno, strerror(errno));
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }

  /* Setup address structure containing address information to use to connect */
  if(is_ipv6) {
    PRINT_DEBUG("is_ipv6 == TRUE");
    struct sockaddr_in6 * saddr6 = (struct sockaddr_in6 *)saddr;
    saddr6->sin6_port = htons(port);
    if(is_udp && is_multicast) {
      PRINT_DEBUG("  is_udp == TRUE && is_multicast == TRUE");
      if(ip == NULL || strlen(ip) < 1) {
        inet_pton(saddr->ss_family, SSDP_ADDR6_LL, &mreq6.ipv6mr_multiaddr);
      }
      else {
        inet_pton(saddr->ss_family, ip, &mreq6.ipv6mr_multiaddr);
      }
      if(interface != NULL && strlen(interface) > 0) {
        mreq6.ipv6mr_interface = (unsigned int)ifindex;
      }
      else {
        mreq6.ipv6mr_interface = (unsigned int)0;
      }
      saddr6->sin6_addr =  mreq6.ipv6mr_multiaddr;
      #ifdef DEBUG___
      {
        char a[100];
        inet_ntop(saddr->ss_family, (void *)&mreq6.ipv6mr_interface, a, 100);
        PRINT_DEBUG("    mreq6->ipv6mr_interface: %s", a);
        inet_ntop(saddr->ss_family, (void *)&mreq6.ipv6mr_multiaddr, a, 100);
        PRINT_DEBUG("    mreq6->ipv6mr_multiaddr: %s", a);
      }
      #endif
      if(setsockopt(sock, protocol, IPV6_MULTICAST_IF, &mreq6.ipv6mr_interface, sizeof(mreq6.ipv6mr_interface)) < 0) {
        PRINT_ERROR("setsockopt() IPV6_MULTICAST_IF: (%d) %s", errno, strerror(errno));
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        free_stuff();
        exit(EXIT_FAILURE);
      }
    }
  }
  else {
    PRINT_DEBUG("is_ipv6 == FALSE");
    struct sockaddr_in *saddr4 = (struct sockaddr_in *)saddr;
    saddr4->sin_port = htons(port);
    if(is_udp && is_multicast) {
      PRINT_DEBUG("  is_udp == TRUE && is_multicast == TRUE");
      if(ip == NULL || strlen(ip) < 1) {
        inet_pton(saddr->ss_family, SSDP_ADDR, &mreq.imr_multiaddr);
      }
      else {
        inet_pton(saddr->ss_family, ip, &mreq.imr_multiaddr);
      }
      if(interface != NULL && strlen(interface) > 0) {
        mreq.imr_interface.s_addr = saddr4->sin_addr.s_addr; //s_addr = (in_addr_t)ifindex ??
      }
      else {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
      }
      saddr4->sin_addr =  mreq.imr_multiaddr;
      #ifdef DEBUG___
      {
        PRINT_DEBUG("    IP_MULTICAST_IF");
        char a[100];
        inet_ntop(saddr->ss_family, (void *)&mreq.imr_interface, a, 100);
        PRINT_DEBUG("    mreq->imr_interface: %s", a);
        inet_ntop(saddr->ss_family, (void *)&mreq.imr_multiaddr, a, 100);
        PRINT_DEBUG("    mreq->imr_multiaddr: %s", a);
      }
      #endif
      if(setsockopt(sock, protocol, IP_MULTICAST_IF, &mreq.imr_interface, sizeof(mreq.imr_interface)) < 0) {
        PRINT_ERROR("setsockopt() IP_MULTICAST_IF: (%d) %s", errno, strerror(errno));
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        free_stuff();
        exit(EXIT_FAILURE);
      }
    }
  }

  /* Enable/disable loopback multicast */
  if(is_multicast && conf->enable_loopback) {
    unsigned char loop = FALSE;
    PRINT_DEBUG("Disabling loopback multicast traffic");
    if(setsockopt(sock, protocol, (is_ipv6 ? IPV6_MULTICAST_LOOP : IP_MULTICAST_LOOP), &loop, sizeof(loop)) < 0) {
      PRINT_ERROR("setupSocket(); setcockopt() IPV(6)_MULTICAST_LOOP: (%d) %s", errno, strerror(errno));
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
  }

  /* If server requested, bind the socket to the given address and port*/
  if(is_server) {
    PRINT_DEBUG("is_server == TRUE");
    char a[100];
    inet_ntop(saddr->ss_family, (void *)&((struct sockaddr_in *)saddr)->sin_addr, a, 100);
    PRINT_DEBUG("  bind() to: saddr->sin_family: %d(%d)", ((struct sockaddr_in *)saddr)->sin_family, AF_INET);
    PRINT_DEBUG("  bind() to: saddr->sin_addr: %s (port %d)", a, ntohs(((struct sockaddr_in *)saddr)->sin_port));

    // TODO: BIND TO INADDR_ANY if sa and interface is NULL !!!!!!!

    if(bind(sock, (struct sockaddr *)saddr, (saddr->ss_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))) < 0) {
      PRINT_ERROR("setupSocket(); bind(): (%d) %s", errno, strerror(errno));
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
    if(!is_udp) {
      PRINT_DEBUG("  is_udp == FALSE");
      if(listen(sock, LISTEN_QUEUE_LENGTH) == SOCKET_ERROR) {
        PRINT_ERROR("setupSocket(); listen(): (%d) %s", errno, strerror(errno));
        close(sock);
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        free_stuff();
        exit(errno);
      }
    }
  }

  /* Join the multicast group */
  // TODO: FAIL SCENARIO FIX: Join on each interface when listening for multicast!!!!
  if(is_multicast) {
    PRINT_DEBUG("  is_multicast == TRUE");
    if(is_ipv6) {
    #ifdef DEBUG___
    {
      PRINT_DEBUG("    is_ipv6 == TRUE");
      PRINT_DEBUG("      IPV6_ADD_MEMBERSHIP");
      char a[100];
      inet_ntop(AF_INET6, (void *)&mreq6.ipv6mr_interface, a, 100);
      PRINT_DEBUG("      mreq6->ipv6mr_interface: %s", a);
    }
    #endif
    if(setsockopt(sock, protocol, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(struct ipv6_mreq)) < 0) {
      PRINT_ERROR("setupSocket(); setsockopt() IPV6_ADD_MEMBERSHIP: (%d) %s", errno, strerror(errno));
      close(sock);
      free(saddr);
      if(sa != NULL) {
        free(interface);
      }
      free_stuff();
      exit(EXIT_FAILURE);
    }
    }
    else {
      #ifdef DEBUG___
      {
        PRINT_DEBUG("    is_ipv6 == FALSE");
        PRINT_DEBUG("      IP_ADD_MEMBERSHIP");
        char a[100];
        inet_ntop(AF_INET, (void *)&mreq.imr_interface, a, 100);
        PRINT_DEBUG("      mreq->imr_interface: %s", a);
        inet_ntop(AF_INET, (void *)&mreq.imr_multiaddr, a, 100);
        PRINT_DEBUG("      mreq->imr_multiaddr: %s", a);
      }
      #endif

      if(setsockopt(sock, protocol, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        PRINT_ERROR("setupSocket(); setsockopt() IP_ADD_MEMBERSHIP: (%d) %s", errno, strerror(errno));
        close(sock);
        free(saddr);
        if(sa != NULL) {
          free(interface);
        }
        free_stuff();
        exit(EXIT_FAILURE);
      }
    }

    mcast_str = (char *)malloc(sizeof(char) * 1024);
    memset(mcast_str, '\0', sizeof(char) * 1024);
    inet_ntop(saddr->ss_family, (is_ipv6 ? (void *)&mreq6.ipv6mr_multiaddr : (void *)&mreq.imr_multiaddr), mcast_str, sizeof(char) * 1024);

    if(!conf->quiet_mode) {
      printf("Joined multicast group %s\n", mcast_str);
    }

    free(mcast_str);
  }

  free(saddr);

  return sock;
}

/**
* Check whether a given IP is a multicast IP
* (if IPV6 checks for UPnP multicast IP, should be
* changed to check if multicast at all)
*
* @param const char *address The address to check
*
* @return BOOL Obvious.
*/
static BOOL isMulticastAddress(const char *address) {
  char *str_first = NULL;
  int int_first = 0;
  BOOL is_ipv6;
  if(address != NULL) {
    is_ipv6 = strchr(address, ':') != NULL ? TRUE : FALSE;
    if(strcmp(address, "0.0.0.0") == 0) {
      return TRUE;
    }
    if(is_ipv6) {
      struct in6_addr ip6_ll;
      struct in6_addr ip6_addr;
      inet_pton(AF_INET6, "ff02::c", &ip6_ll);
      inet_pton(AF_INET6, address, &ip6_addr);
      if(ip6_ll.s6_addr == ip6_addr.s6_addr) {
        return TRUE;
      }
      return FALSE;
    }
    else {
      str_first = (char *)malloc(sizeof(char) * 4);
      memset(str_first, '\0', sizeof(char) * 4);
      strncpy(str_first, address, 3);
      int_first = atoi(str_first);
      free(str_first);
      if(int_first > 223 && int_first < 240) {
        return TRUE;
      }
      return FALSE;
    }
  }
  printf("Supplied address ('%s') is not valid\n", address);
  free_stuff();
  exit(EXIT_FAILURE);
  return FALSE;
}

/**
* Initializes (allocates neccessary memory) for a SSDP message
*
* @param message The message to initialize
*/
static void init_ssdp_message(ssdp_message_s *message) {
  message->mac = (char *)malloc(sizeof(char) * MAC_STR_MAX_SIZE);
  memset(message->mac, '\0', MAC_STR_MAX_SIZE);
  message->ip = (char *)malloc(sizeof(char) * IPv6_STR_MAX_SIZE);
  memset(message->ip, '\0', IPv6_STR_MAX_SIZE);
  message->datetime = (char *)malloc(sizeof(char) * 20);
  memset(message->datetime, '\0', 20);
  message->request = (char *)malloc(sizeof(char) * 1024);
  memset(message->request, '\0', 1024);
  message->protocol = (char *)malloc(sizeof(char) * 48);
  memset(message->protocol, '\0', sizeof(char) * 48);
  message->answer = (char *)malloc(sizeof(char) * 1024);
  memset(message->answer, '\0', sizeof(char) * 1024);
  message->header_count = 0;
}

/**
* Create a plain-text message
*
* @param results The buffer to store the results in
* @param buffer_size The size of the buffer 'results'
* @param message The message to convert
*/
static BOOL create_plain_text_message(char *results, int buffer_size, ssdp_message_s *ssdp_message) {
  int buffer_used = 0;
  buffer_used += snprintf(results + buffer_used,
        buffer_size - buffer_used,
        "Time received: %s\n",
        ssdp_message->datetime);
  buffer_used += snprintf(results + buffer_used,
        buffer_size - buffer_used,
        "Origin-MAC: %s\n",
        (ssdp_message->mac != NULL ? ssdp_message->mac : "(Could not be determined)"));
  buffer_used += snprintf(results + buffer_used,
        buffer_size - buffer_used,
        "Origin-IP: %s\nMessage length: %d Bytes\n",
        ssdp_message->ip,
        ssdp_message->message_length);
  buffer_used += snprintf(results + buffer_used,
        buffer_size - buffer_used,
        "Request: %s\nProtocol: %s\n",
        ssdp_message->request,
        ssdp_message->protocol);

  if(conf->fetch_info) {
    int bytes_fetched = 0;
    char *fetched_string = (char *)malloc(sizeof(char) * buffer_size);
    memset(fetched_string, '\0', buffer_size);
    bytes_fetched = fetch_upnp_device_info(ssdp_message, fetched_string, buffer_size);
    if(bytes_fetched > 0) {
      buffer_used += snprintf(results + buffer_used,
                buffer_size - buffer_used,
                "%s",
                fetched_string);
    }
    free(fetched_string);
  }

  int hc = 0;
  ssdp_header_s *ssdp_headers = ssdp_message->headers;
  while(ssdp_headers) {
  buffer_used += snprintf(results + buffer_used,
          buffer_size - buffer_used,
          "Header[%d][type:%d;%s]: %s\n",
          hc, ssdp_headers->type,
          get_header_string(ssdp_headers->type, ssdp_headers),
          ssdp_headers->contents);
    ssdp_headers = ssdp_headers->next;
    hc++;
  }

  ssdp_headers = NULL;

  return TRUE;
}

#ifdef DEBUG___
/**
* Counts the number of times needle occurs in haystack
*
* @param haystack The string to search in
* @param needle The character to search for
*
* @return int The number of occurrences
*/
static int chr_count(char *haystack, char needle) {
  int count = 0;
  int len = strlen(haystack);
  int i;
  for (i = 0; i < len; i++) {
    if (haystack[i] == needle) {
      count++;
    }
  }
  return count;
}

/**
* Prints a debug message, mimicking printf-like function, supports only %d, %s and %c
*
* @param const char* file The file the debug message generated in
* @param int line The line number in the file the debug message generated at
* @param const char* message The debug message to be displayed
* @param ... variable number of optional arguments that match with the va_format
*/

static void print_debug(FILE *std, const char *color, const char* file, int line, char *va_format, ...) {
  va_list va;
  char *message = NULL;
  int message_used = 0;
  char *from_pointer = va_format;
  char *to_pointer = va_format;
  int args_count = chr_count(va_format, '%');
  const char *no_memory = "Not enought memory in buffer to build debug message";
  char *s = NULL;
  char c;
  int d;

  if(!va_format) {
    fprintf(stderr, "%s[%d][%s:%d] Error, va_arg input format (va_format) not set%s\n", (color?color:ERROR_COLOR_BEGIN), (int)getpid(), __FILE__, __LINE__, DEBUG_COLOR_END);
    return;
  }

  int message_length = 10240;
  message = (char *)malloc(sizeof(char) * message_length);
  memset(message, '\0', sizeof(char) * message_length);
  message_length -= 50; // compensate for additional chars in the message
  va_start(va, va_format);

  int copy_length = 0;
  int args_pos;

  for (args_pos = 1; (from_pointer && *from_pointer != '\0'); from_pointer = to_pointer) {

    to_pointer = strchr(from_pointer, '%');

    if(to_pointer && strlen(to_pointer) > 1) {
      copy_length = to_pointer - from_pointer;

      if(copy_length > message_length - message_used) {
      fprintf(stderr, "%s[%d][%s:%d] %s%s\n", (color?color:ERROR_COLOR_BEGIN), (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
        free(message);
        return;
      }

      strncpy(message + message_used, from_pointer, copy_length);
      message_used += copy_length;

  switch(to_pointer[1]) {
  case 's':
    /* string */
    s = va_arg(va, char *);
    if(s) {
      copy_length = strlen(s);
      if(copy_length > message_length - message_used) {
        fprintf(stderr, "%s[%d][%s:%d] %s%s\n", (color?color:ERROR_COLOR_BEGIN), (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
        free(message);
        return;
      }
      strncpy(message + message_used, s, copy_length);
      message_used += copy_length;
    }
    else {
      copy_length = 6;
      strncpy(message + message_used, "NULL", copy_length);
      message_used += copy_length;
    }
    break;
  case 'c':
    /* character */
    c = (char)va_arg(va, int);
    copy_length = 1;
    if(copy_length > message_length - message_used) {
      fprintf(stderr, "%s[%d][%s:%d] %s%s\n", (color?color:ERROR_COLOR_BEGIN), (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
      free(message);
      return;
    }
    strncpy(message + message_used, &c, copy_length);
    message_used += copy_length;
    break;
  case 'd':
    /* integer */
    d = va_arg(va, int);
    char *d_char = (char *)malloc(sizeof(char) * 10);
    memset(d_char, '\0', 10);
    sprintf(d_char, "%d", d);
    copy_length = strlen(d_char);
    if(copy_length > message_length - message_used) {
      fprintf(stderr, "%s[%d][%s:%d] %s%s\n", (color?color:ERROR_COLOR_BEGIN), (int)getpid(), __FILE__, __LINE__, no_memory, DEBUG_COLOR_END);
      free(d_char);
      free(message);
      return;
    }
    strncpy(message + message_used, d_char, copy_length);
    message_used += copy_length;
    free(d_char);
    break;
  default:
    break;
  }

    to_pointer += 2;
  }
  else {
    strcat(message, from_pointer);
    to_pointer = NULL;
  }

  if(args_pos > args_count) {
    break;
  }
    args_pos++;
  }

  if(!std) {
    std = stdout;
  }

  fprintf(std, "%s[%d][%s:%d] %s%s\n", color, (int)getpid(), file, line, message, DEBUG_COLOR_END);

  free(message);
}

static void add_alloc(const char *alloc_name) {
  if(!alloc_list) {
    alloc_list = (char **)malloc(sizeof(char *) * 500);
  }
  char *alloc_element = (char *)malloc(sizeof(char) * 50);
  strncpy(alloc_element, alloc_name, 50);
  alloc_list[alloc_list_size++] = alloc_element;
  PRINT_DEBUG("Adding alloc '%s'", alloc_name);
}

static void remove_alloc(const char *alloc_name) {
  int i = 0;
  for(; i < alloc_list_size; i++) {
    if(0 == strcmp(alloc_name, alloc_list[i])) {
      strcat(alloc_list[i], " is freed");
      PRINT_DEBUG("Removing alloc '%s'", alloc_name);
      return;
    }
  }
  PRINT_DEBUG("Failed to remove alloc '%s'", alloc_name);
}

static void print_alloc() {
  int i = 0;
  for(; i < alloc_list_size; i++) {
    PRINT_DEBUG("%d: '%s'", i, alloc_list[i]);
  }
}
#endif

