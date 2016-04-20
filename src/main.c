#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "common.h"

void usage();

int main(int argc, char **argv)
{	
	params cmd;
	
	const char* short_options = "hf:";
	char *ftokFilePath=NULL;

	const struct option long_options[] =
	{
		{"help",no_argument,NULL,'h'},
		{"ftok",required_argument,NULL,'f'},
		{NULL,0,NULL,0}
	};

	int rez;
	int option_index;

	while ((rez=getopt_long(argc,argv,short_options,
		long_options,&option_index))!=-1){

		switch(rez){
			case 'h': 
			{
				usage();
				break;
			};
			case 'f':
			{
				ftokFilePath = malloc(strlen(optarg)+1);
				strcpy(ftokFilePath,optarg);
				break;
			};
			default:
			{
				fprintf(stderr,"[E]: Found unknown option\n");
				usage();
				break;
			};
		};
	};
	
	if(ftokFilePath == NULL)
	{
		printf("[E]: You should set --ftok parameter\n");
		usage();
	}
	
	cmd.ftokFilePath = ftokFilePath;
	
	proc_manager(&cmd);
	
	return 0;
}

void usage()
{
	printf(" -h or --help show this message and exit.\n");
	printf(" -f or --ftok /path/to/file\n");
	exit(EXIT_SUCCESS);
};
