/*
 * Modified by davelv 2011-11-03
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbenchpro --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, pthread failed
 *    4 - resourse limits
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <rpc/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include "socket.h"


#define PROGRAM_VERSION "0.1"
#define METHOD_GET	0
#define METHOD_HEAD	1
#define METHOD_OPTIONS	2
#define METHOD_TRACE	3
#define HTTP_09		0
#define HTTP_10		1
#define HTTP_11		2
#define REQUEST_SIZE	2048

#define DEFAULT_CLIENTS	1
#define DEFAULT_TIME	30
#define DEFAULT_FORCE	0
#define DEFAULT_PORT	80
#define DEFAULT_METHOD	METHOD_GET

#define DEFAULT_FORE_RELOAD	0
#define DEFAULT_HTTP_VER	HTTP_11
/* globals */
int g_http_ver= DEFAULT_HTTP_VER;
int g_method = DEFAULT_METHOD;
int g_clients = DEFAULT_CLIENTS;
int g_force = DEFAULT_FORCE;
int g_force_reload = DEFAULT_FORE_RELOAD;
int g_port = DEFAULT_PORT;
int g_bench_time = DEFAULT_TIME;
char *g_proxyhost = NULL;
char g_host[MAXHOSTNAMELEN];
char g_request[REQUEST_SIZE];

struct thread_arg{
	char *host;
	int  port;
	char *request;
	unsigned long long speed;
	unsigned long long failed;
	unsigned long long httperr;
	unsigned long long bytes;
};
	

/* prototypes */
static void *bench_thread(void *para);
static int bench(void);
static void build_request(const char *url);
static int  http_response_check(const char *response);
static int parse_opt(int argc, char *argv[]);
static int resource_set(int clients);


static int 
resource_set(int clients)
{
	struct rlimit lim;
	static const int used_lim = 128;
	
	getrlimit(RLIMIT_NOFILE, &lim);//get file des limit
	if (lim.rlim_cur < (rlim_t)(used_lim + clients))
	{	//set file des limit
		lim.rlim_cur = used_lim + clients;
		lim.rlim_max = used_lim + clients;
		if (setrlimit(RLIMIT_NOFILE, &lim))
			return -1;
	}
	
	getrlimit(RLIMIT_NPROC, &lim);//get file des limit
	if (lim.rlim_cur < (rlim_t)(used_lim + clients))
	{	//set file des limit
		lim.rlim_cur = used_lim + clients;
		lim.rlim_max = used_lim + clients;
		if (setrlimit(RLIMIT_NPROC, &lim))
			return -2;
	}
	
	return 0;
}
static void
exit_handler(int signal)
{
        if (signal == SIGUSR1)
		pthread_exit(NULL);
}

static void
usage(void)
{
        fprintf(stderr,
                "webbenchpro [option]... URL\n"
		"  -f|--force                      Don't wait for reply from server.\n"
                "  -r|--reload                     Send reload request - Pragma: no-cache.\n"
                "  -t|--time <sec>                 Run benchmark for <sec> seconds. Default 30.\n"
                "  -p|--proxy <server:port>        Use proxy server for request.\n"
                "  -c|--clients <n>                Run <n> HTTP clients at once. Default one.\n"
                "  -9|--http09                     Use HTTP/0.9 style requests.\n"
                "  -1|--http10                     Use HTTP/1.0 protocol.\n"
                "  -2|--http11                     Use HTTP/1.1 protocol.\n"
                "  --get                           Use GET request method.\n"
                "  --head                          Use HEAD request method.\n"
                "  --options                       Use OPTIONS request method.\n"
                "  --trace                         Use TRACE request method.\n"
                "  -?|-h|--help                    This information.\n"
                "  -V|--version                    Display program version.\n"
               );
};
static int 
parse_opt(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"g_force",	no_argument,		&g_force,		1},
		{"reload",	no_argument,		&g_force_reload,	1},
		{"get",		no_argument,		&g_method,	METHOD_GET},
		{"head",	no_argument,		&g_method,	METHOD_HEAD},
		{"options",	no_argument,		&g_method,	METHOD_OPTIONS},
		{"trace",	no_argument,		&g_method,	METHOD_TRACE},
		{"help",	no_argument,		NULL,		'?'},
		{"http09",	no_argument,		NULL,		'9'},
		{"http10",	no_argument,		NULL,		'1'},
		{"http11",	no_argument,		NULL,		'2'},
		{"version",	no_argument,		NULL,		'V'},
		{"time",	required_argument,	NULL,		't'},
		{"proxy",	required_argument,	NULL,		'p'},
		{"clients",	required_argument,	NULL,		'c'},
		{NULL,		0,			NULL,		0}
	};
        
	int opt = 0;
        int options_index = 0;
        char *tmp = NULL;

        if (argc == 1) {
                usage();
                return 0;
        }

        while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?h",long_options,&options_index))!=EOF ) {
                switch(opt) {
			case  0 :
				break;
			case 'f':
				g_force = 1;
				break;
			case 'r':
				g_force_reload = 1;
				break;
			case '9':
				g_http_ver = HTTP_09;
				break;
			case '1':
				g_http_ver = HTTP_10;
				break;
			case '2':
				g_http_ver = HTTP_11;
        	                break;
	                case 'V':
	                        printf(PROGRAM_VERSION"\n");
	                        return 0;
	                case 't':
	                        g_bench_time=atoi(optarg);
	                        break;
	                case 'p':
	                        /* proxy server parsing server:port */
        	                tmp=strrchr(optarg,':');
                	        g_proxyhost=optarg;
                        	if (tmp==NULL) {
	                                break;
	                        }
        	                if(tmp==optarg) {
                	                fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
                        	        return -1;
	                        }
        	                if (tmp==optarg+strlen(optarg)-1) {
                	                fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
                        	        return -1;
	                        }
        	                *tmp='\0';
                	        if ( (g_port=atoi(tmp+1) < 0)) {
					fprintf(stderr,"Error in option --proxy %s Port number is invaild.\n",optarg);
					return -1;
				}
                        	break;
	                case ':':
        	        case 'h':
                	case '?':
	                        usage();
        	                return 0;
                	        break;
	                case 'c':
        	                g_clients=atoi(optarg);
                	        break;
		}
	}

        if (optind==argc) {
                fprintf(stderr,"webbenchpro: Missing URL!\n");
                usage();
                return 2;
        }

        if (g_clients <= 0)
		g_clients = DEFAULT_CLIENTS;
        if (g_bench_time <= 0) 
		g_bench_time = DEFAULT_TIME;
	return 1;
}

int
main(int argc, char *argv[])
{
	int ret;
        // parse options
        if  ( (ret=parse_opt(argc, argv)) <=0 )
		return  ret<0 ? 2 : 0;
        
	build_request(argv[argc-1]);
	/* Copyright */
        printf(	"\nWebBenchPro - Advanced Simple Web Benchmark "PROGRAM_VERSION"\n"
                "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
		"Modified By Davelv 2011-11-03\n"
               );
	//check resourse limits
	if ( (ret=resource_set(g_clients)) <0 )
	{
		fprintf(stderr, "\nSet %s  limit failed. \n"
			"Try less clients or use higher authority or set \"ulimit -n -u\" manualy\n",
			ret==-1 ? "NOFILE" : "NPROC");
		return 4;
	}
        /* print bench info */
        printf("\nBenchmarking: ");
        switch(g_method) {
        	case METHOD_GET:
        	default:
                	printf("GET");
                	break;
        	case METHOD_OPTIONS:
                	printf("OPTIONS");
                	break;
	        case METHOD_HEAD:
        	        printf("HEAD");
                	break;
	        case METHOD_TRACE:
        	        printf("TRACE");
                	break;
	        }
        printf(" %s",argv[argc-1]);
        switch(g_http_ver) {
        case 0:
                puts(" (using HTTP/0.9)"); break;
	case 1:
		puts(" (using HTTP/1.0)"); break;
        case 2:
                puts(" (using HTTP/0.9)"); break;
        }
        return bench();
}

void build_request(const char *url)
{
        char tmp[10];
        int i;

        bzero(g_host,MAXHOSTNAMELEN);
        bzero(g_request,REQUEST_SIZE);

        if (g_force_reload && g_proxyhost!=NULL && g_http_ver<1) g_http_ver=1;
        if (g_method==METHOD_HEAD && g_http_ver<1) g_http_ver=1;
        if (g_method==METHOD_OPTIONS && g_http_ver<2) g_http_ver=2;
        if (g_method==METHOD_TRACE && g_http_ver<2) g_http_ver=2;

        switch(g_method) {
        default:
        case METHOD_GET:
                strcpy(g_request,"GET");
                break;
        case METHOD_HEAD:
                strcpy(g_request,"HEAD");
                break;
        case METHOD_OPTIONS:
                strcpy(g_request,"OPTIONS");
                break;
        case METHOD_TRACE:
                strcpy(g_request,"TRACE");
                break;
        }

        strcat(g_request," ");

        if (NULL==strstr(url,"://")) {
                fprintf(stderr, "\n%s: is not a valid URL.\n",url);
                exit(2);
        }
        if (strlen(url)>1500) {
                fprintf(stderr,"URL is too long.\n");
                exit(2);
        }
        if (g_proxyhost==NULL)
                if (0!=strncasecmp("http://",url,7)) {
                        fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
                        exit(2);
                }
        /* protocol/host delimiter */
        i=strstr(url,"://")-url+3;
        /* printf("%d\n",i); */

        if (strchr(url+i,'/')==NULL) {
                fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
                exit(2);
        }
        if (g_proxyhost==NULL) {
                /* get port from hostname */
                if (index(url+i,':')!=NULL &&
                                index(url+i,':')<index(url+i,'/')) {
                        strncpy(g_host,url+i,strchr(url+i,':')-url-i);
                        bzero(tmp,10);
                        strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
                        /* printf("tmp=%s\n",tmp); */
                        g_port=atoi(tmp);
                        if (g_port==0) g_port=80;
                } else {
                        strncpy(g_host,url+i,strcspn(url+i,"/"));
                }
                // printf("Host=%s\n",host);
                strcat(g_request+strlen(g_request),url+i+strcspn(url+i,"/"));
        } else {
                // printf("ProxyHost=%s\nProxyPort=%d\n",g_proxyhost,g_port);
                strcat(g_request,url);
        }
        if (g_http_ver==1)
                strcat(g_request," HTTP/1.0");
        else if (g_http_ver==2)
                strcat(g_request," HTTP/1.1");
        strcat(g_request,"\r\n");
        if (g_http_ver>0)
                strcat(g_request,"User-Agent: WebBenchPro "PROGRAM_VERSION"\r\n");
        if (g_proxyhost==NULL && g_http_ver>0) {
                strcat(g_request,"Host: ");
                strcat(g_request,g_host);
                strcat(g_request,"\r\n");
        }
        if (g_force_reload && g_proxyhost!=NULL) {
                strcat(g_request,"Pragma: no-cache\r\n");
        }
        if (g_http_ver>1)
                strcat(g_request,"Connection: close\r\n");
        /* add empty line at end */
        if (g_http_ver>0) strcat(g_request,"\r\n");
        // printf("Req=%s\n",g_request);
}

/* vraci system rc error kod */
static int bench(void)
{
	int i;
	unsigned long long speed=0;
	unsigned long long failed=0;
	unsigned long long httperr=0;
	unsigned long long bytes=0;
	// set thread args
	struct thread_arg *args = malloc (sizeof(struct thread_arg)*g_clients);
	if (args == NULL){
                fprintf(stderr,"Malloc thread args failed. Aborting benchmark.\n");
                return 3;
        }	
	memset(args, 0, sizeof(args));
	for (i=0; i<g_clients; i++)
	{
		args[i].host = g_proxyhost==NULL?g_host:g_proxyhost;
		args[i].port = g_port;
		args[i].request = g_request;
	}
	//theads
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	if (pthread_attr_setstacksize(&attr, 32768)){;//stack size 32K
		fprintf(stderr,"Set stack size failed . Aborting benchmark.\n");
		return 3;
	}
	pthread_t *threads= malloc(sizeof(pthread_t)*(g_clients));
	if (threads == NULL){
		fprintf(stderr,"Malloc threads failed. Aborting benchmark.\n");
		return 3;
	}
        /* check avaibility of target server */
	i = Socket(g_proxyhost==NULL?g_host:g_proxyhost, g_port);
        if (i<0) {
                fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
                return 1;
        }
        close(i);
        printf("\n%d clients, running %d sec",g_clients, g_bench_time);
        if (g_force) printf(", early socket close");
        if (g_proxyhost!=NULL) printf(", via proxy server %s:%d",g_proxyhost,g_port);
        if (g_force_reload) printf(", forcing reload");
        printf(".\n");
        /* setup alarm signal handler */
        struct sigaction  sa_exit;
	
        sa_exit.sa_handler=exit_handler;
        sa_exit.sa_flags=0;
        if (sigaction(SIGUSR1,&sa_exit,NULL))
                exit(3);
	//new thread
	for (i=0; i<g_clients; i++)
	{
		int ret_p =pthread_create(threads+i, &attr, bench_thread, args+i);
		if (ret_p)
		{
			printf("pthread create error %d on %d\n", ret_p, i);
			i-- , g_clients--;
			//exit (-1);
		}
	}
	//main thread sleep
	sleep(g_bench_time);
	//cancle threads
	for (i=0; i<g_clients; i++)
	{
		pthread_kill(threads[i], SIGUSR1);
		//pthread_cancel(threads[i]);
	}
	//join & calc the result
	for (i=0; i<g_clients; i++)
	{
		pthread_join(threads[i], NULL);	
		//printf("%llu,%llu,%llu,%llu\n", args[i].speed, args[i].httperr, args[i].failed, args[i].bytes);
		speed += args[i].speed;
		httperr += args[i].httperr;
		failed += args[i].failed;
		bytes += args[i].bytes;
	}
        printf("\nSpeed=%llu pages/sec, %llu bytes/sec.\nRequests: %llu ok, %llu http error, %llu failed.\n",
               (speed+httperr+failed)/g_bench_time, bytes/g_bench_time , speed, httperr, failed);

	return 0;
}

#define ERR_PROCESS(a, b, c) {a++; close(b); goto c;}
void *bench_thread(void *arg)
{
	struct thread_arg *p_arg = (struct thread_arg *)arg;
        int rlen = strlen(p_arg->request);
        char buf[1500];
        int s,i,cnt;
        
	while(1) {
outloop:	s=Socket(p_arg->host,p_arg->port);
                if (s<0) {
                        p_arg->failed++;
                        continue;
                }
                if (rlen!=write(s,p_arg->request,rlen)) {
                        ERR_PROCESS (p_arg->failed, s, outloop);
                }
                if (g_http_ver==0)
                        if (shutdown(s,1)) {
                        	ERR_PROCESS (p_arg->failed, s, outloop);
                        }
                if (g_force == 0) {
                        /* read all available data from socket */
			cnt = 0;
                        while (1) {
				i = read(s, buf, 1500);
                                if (i < 0) {
                        		ERR_PROCESS (p_arg->failed, s, outloop);
				} 
				else if (i == 0) {
					if (cnt == 0) {//first read
                        			ERR_PROCESS (p_arg->httperr, s, outloop);
					} 
					break;
				}
                                else{	//check http status
                                        p_arg->bytes += i;
					if ((cnt++ == 0)&&(http_response_check(buf))) {
                        			ERR_PROCESS (p_arg->httperr, s, outloop);
					}
				}
                        }
                }
                if (close(s)) {
                        p_arg->failed++;
                        continue;
                }
                p_arg->speed++;
        }
	//never return  from here
	return NULL;
}
int
http_response_check(const char *response)
{
	int status;
	float version;
	//HTTP/
	response+=5;
	if ( (sscanf(response, "%f %d",&version,&status)==2) &&
		(status>=200 || status <300))
		return 0;

	else	return -1;

}
