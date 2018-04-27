#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <git2.h>

extern int errno;
int exitflag;
char login[128];
char* filename;
int filesize;
char* contents;
FILE* fp;
char cwd[1024];
int yash_cd(char **args);
int yash_help(char **args);
int yash_exit(char **args);
int yash_history(char **args);
int yash_editor(char **args);
int yash_alias(char **args);
int yash_downimage(char **args);
int yash_gitclone(char **args);
struct history{
	char* command;
	int pid;
	time_t time;
};
struct alias{
	char* name;
	char* value;
}*aliases;
int n_aliases;
struct history history[25];
int historyptr;
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "history",
  "editor",
  "alias",
  "download",
  "gitclone",
};

int (*builtin_func[]) (char **) = {
  &yash_cd,
  &yash_help,
  &yash_exit,
  &yash_history,
  &yash_editor,
  &yash_alias,
  &yash_downimage,
  &yash_gitclone,
};
int yash_gitclone(char** args){
	printf("Want to clone %s to %s\n",args[1],args[2]);
	int status;
	pid_t pid=fork();
	if(pid==0){
		// Child
		if(args[2]==NULL){
			printf("Need a second argument (file name)\n");
			return 0;
		}
		git_libgit2_init();
		git_repository *repo = NULL;
		int error = git_clone(&repo, args[1], args[2], NULL);
		if(error){
			printf("git_clone function call error\n");
		}
		git_repository_free(repo);
		git_libgit2_shutdown();
		exit(EXIT_SUCCESS);
	}
	else if(pid>0){
		// Parent
		do {
	    	waitpid(pid, &status, WUNTRACED);
	    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
	else{
		printf("fork error in yash_gitclone\n");
	}
	printf("Cloned %s to %s\n",args[1],args[2]);
	return 1;
}
int yash_downimage(char** args){
	printf("Want to download %s\n",args[1]);
	int status;
	pid_t pid=fork();
	if(pid==0){
		// Child
		CURL *curl;
		CURLcode res;

		curl = curl_easy_init();
		if(curl) {
			if(args[2]==NULL){
				printf("Need a second argument (file name)\n");
				return 0;
			}
			
			int fd=open(args[2], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
			dup2(fd, 1);   // make stdout go to file

			curl_easy_setopt(curl, CURLOPT_URL, args[1]);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			// Perform the request, res will get the return code  
			res = curl_easy_perform(curl);
			// Check for errors  
			if(res != CURLE_OK)
			  fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
			// always cleanup
			curl_easy_cleanup(curl);
			close(fd);
		}
		else{
			printf("Curl error\n");
		}
		exit(EXIT_SUCCESS);
	}
	else if(pid>0){
		// Parent
		do {
	    	waitpid(pid, &status, WUNTRACED);
	    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
	else{
		printf("fork error in yash_downimage\n");
	}
	printf("Downloaded %s\n",args[1]);
	return 1;
}
int yash_alias(char **args){
	// Handle undesirable splitting of text in single quotes
	char* combined=strdup(args[1]);
	if(args[2]!=NULL){
		int i=2;
		combined=realloc(combined,sizeof(combined)+1+sizeof(args[i]));
		strcat(combined," ");
		strcat(combined,args[i]);
		args[i]=NULL;
		while(args[i+1]!=NULL){
			i++;
			combined=realloc(combined,sizeof(combined)+1+sizeof(args[i]));
			strcat(combined," ");
			strcat(combined,args[i]);
			args[i]=NULL;
		}
	}
	int i=0;
	char* token = strtok(combined,"=");
	for(;i<n_aliases;i++)
		if(strcmp(aliases[i].name,token)==0)
			break;
	if(i==n_aliases){
		aliases=realloc(aliases,(n_aliases+1)*sizeof(struct alias));
		aliases[n_aliases].name=strdup(token);
	}
	token = strtok(NULL,"=");
	token = strdup(token+1);				// Remove leading single quotes
	char* val=malloc(strlen(token)-1);
	
	strncpy(val,token,strlen(token)-1);	// Remove trailing single quotes
	printf("%s",val);
	aliases[i].value=strdup(val);
	if(i==n_aliases)
		n_aliases++;
	free(token);
	free(combined);
	free(val);
	
	return 1;
}
void insertintohistorypid(int pid){
	history[historyptr].pid=pid;
  	if(historyptr!=24)
    		historyptr++;
}
int isalias(char key[]){
	int i=0;
	for(;i<n_aliases;i++){
		if(strcmp(key,aliases[i].name)==0){
		    char** args=NULL;
		    char* token = strtok(aliases[i].value," ");
		    args = (char**)malloc(sizeof(char*));
		    args[0] = strdup(token);
		    printf("args[0] %s",token);
		    i=1;
		    while(token!=NULL){
		    	token=strtok(NULL," ");
		    	args = (char**)realloc(args,(i+1)*sizeof(char*));
		    	if(token!=NULL){
		    		args[i] = strdup(token);
		    	}
		    	else{
		    		args[i]=NULL;
		    	}
		    	i++;
		    }
			int n_builtin = sizeof(builtin_str) / sizeof(char *);
			int j;
			for (j = 0; i < n_builtin; i++) {
		    	if (strcmp(args[0], builtin_str[j]) == 0) {
			    	return (*builtin_func[j])(args);
			    }
			}
			pid_t pid;
			int status;
			printf("%s \n",args[0]);
			pid = fork();
			if (pid == 0) {
			    // Child process
			    if (execvp(args[0], args) == -1) {
			    	perror("yash: Child process successfully created. But cannot execvp() the command entered. Check Google for more help. Stackoverflow will do.");
			    }
			    exit(EXIT_FAILURE);
			} 
			else if (pid < 0) {
			    perror("yash: Error creating Child process.");
			} 
			else {
			    // Parent process
			    insertintohistorypid(pid);
			    do {
			    	waitpid(pid, &status, WUNTRACED);
			    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
			}
			return 1;
		}
	}
	return 0;
}
void pop(){
  int i;
  for(i=0;i<(25-1);i++){
    history[i]=history[i+1];
  }
}
void insertintohistory(char* line){
  if(historyptr==24)  pop();
	history[historyptr].command=strdup(line);
	history[historyptr].time=time(0);
}
int yash_history(char** args){
	int i;
	for(i=0;i<historyptr-1;i++){
		// Print previous commands
    printf("%s %d %s\n",history[i].command,history[i].pid,ctime(&history[i].time));
	}
  return 1;
}
void savetofile(){
	exitflag = 1;
	fseek(fp,0,SEEK_SET);
	fwrite(contents,filesize,1,fp);
	printf("\n");
	printf("Exiting %s \n",filename);
}
int yash_editor(char** args){
	int exist =access(args[1],F_OK);
	if(exist==-1){
		fp=fopen(args[1] ,"a");
		fclose(fp);
	}
	fp = fopen(args[1],"r+");
	filename = strdup(args[1]);
	system("clear");
	printf("Yet Another Editor v0.0.1. Opened file - %s Press Enter after text, then Ctrl + C and then ENTER to exit editor\n",args[1]);
	fseek(fp,0,SEEK_END);
	filesize = ftell(fp);
	fseek(fp,0,SEEK_SET);
	contents=malloc(filesize);
	fread(contents,filesize,1,fp);
	printf("%s",contents);
	while(!exitflag){
		signal(SIGINT,&savetofile);
		contents=realloc(contents,filesize+1);
		scanf("%c",(contents+filesize));
		filesize++;
	}
	free(contents);
	filesize=0;
	fclose(fp);
	exitflag = 0;
	return 1;
}
#define YASH_TOK_BUFSIZE 64
#define YASH_TOK_DELIM " \t\r\n\a"
char ** yash_split_line(char *line){
  int bufsize = YASH_TOK_BUFSIZE;
  int position = 0;
  // Allocation of size bufsize is an overestimation. Upper bound.
  char **tokens = malloc(bufsize * sizeof(char*));	
  char *token, **tokens_copy;

  if (!tokens) {
    perror("yash: token buffer cannot be allocated.");
    exit(EXIT_FAILURE);
  }
  token = strtok(line, YASH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;
    if (position == bufsize) {
      bufsize += YASH_TOK_BUFSIZE;
      tokens_copy = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
		free(tokens_copy);
        perror("yash: token buffer cannot be reallocated.");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, YASH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}
int yash_cd(char **args){
	if (args[1] == NULL) {
		fprintf(stderr, "yash: expected argument for cd command. Use man to check arguments.\n");
		return 1;
	} 
	char* combined=strdup(args[1]);
	if(args[2]!=NULL){
		int i=2;
		combined=realloc(combined,sizeof(combined)+2+sizeof(args[i]));
		strcat(combined," ");
		strcat(combined,args[i]);
		args[i]=NULL;
		while(args[i+1]!=NULL){
			i++;
			combined=realloc(combined,sizeof(combined)+2+sizeof(args[i]));
			strcat(combined," ");
			strcat(combined,args[i]);
			args[i]=NULL;
		}
	}
	if (chdir(combined) != 0) {
		if(errno==EACCES)	perror("yash: error changing directory.  Search permission is denied for one of the components  of  path.(See also path_resolution(7).)");
		if(errno==EFAULT)	perror("yash: error changing directory.  path points outside your accessible address space.");
		if(errno==EIO)	perror("yash: error changing directory.  An I/O error occurred.");
		if(errno==ELOOP)	perror("yash: error changing directory.  Too many symbolic links were encountered in resolving path.");
		if(errno==ENAMETOOLONG)	perror("yash: error changing directory.  path is too long.");
		if(errno==ENOENT)	perror("yash: error changing directory.  The file does not exist.");
		if(errno==ENOMEM)	perror("yash: error changing directory.  Insufficient kernel memory was available.");
		if(errno==ENOTDIR)	perror("yash: error changing directory.  A component of path is not a directory.");
		if(errno==EBADF)	perror("yash: error changing directory.  fd is not a valid file descriptor.");
	}
	getcwd(cwd, sizeof(cwd));
	return 1;
}
int yash_help(char **args){
	int i;
	printf("Yet Another SHell\n");
	printf("Type program names and arguments, and hit enter to execute.\n");
	printf("The following are built in:\n");
	int n_builtin = sizeof(builtin_str) / sizeof(char *);
	for (i = 0; i < n_builtin; i++) {
		printf("  %s\n", builtin_str[i]);
	}
	printf("Use the man command for information on other programs.\n");
	return 1;
}
int yash_exit(char **args){
	free(aliases);
	return 0;
}
void run(char *cmd) {
   	// Incomplete. Handle parameters to cmds
    char *args[2];
    args[0] = cmd;
    args[1] = NULL;
    execvp(cmd, args);
}
int piping(char* line){
	FILE* fp;
	int status;
	fp=popen(line,"w");
	if(fp==NULL){
		printf("fp NULL\n");
	}
	status=pclose(fp);
	if(status==-1){
		printf("status -1\n");
	}
	return 1;
}
int ioredir(char* line){		// Can't handle grep        <          ab.txt           or similar multi space cmds
	char* cmdline;
	char** cmd;
	int status,in,out;
	char* input=NULL,*output=NULL;
	pid_t pid;
	char* temp=strdup(line);
	char* token = strtok(temp,"<");
	if(strlen(token)!=strlen(line)){		// Just input
		in=1;
		if(token[strlen(token)-1]==' ')		// Remove space
			cmdline=strndup(token,strlen(token)-1);
		token = strtok(NULL,"<");
		cmd = yash_split_line(cmdline);
		if(token[0]==' ')
			token+=1;
		input=strdup(token);
	}
	temp=strdup(line);
	token = strtok(temp,">");
	if(strlen(token)!=strlen(line)){		// Just output
		out=1;
		if(token[strlen(token)-1]==' ')		// Remove space
			cmdline=strndup(token,strlen(token)-1);
		token = strtok(NULL,">");
		cmd = yash_split_line(cmdline);
		if(token[0]==' ')
			token+=1;
		output=strdup(token);
	}
	printf("%s\n",cmdline);
	if(input!=NULL)
		printf("%s\n", input);
	if(output!=NULL)
		printf("%s\n",output );
	if ((pid = fork()) < 0)
    	perror("Fork error");
	else if (pid == 0){
	    if (in){
	        int fd0 = open(input, O_RDONLY);
	        dup2(fd0, STDIN_FILENO);
	        close(fd0);
	    }
	    if (out)
	    {
	        int fd1 = creat(output , 0644) ;
	        dup2(fd1, STDOUT_FILENO);
	        close(fd1);
	    }
	    execvp(cmd[0], cmd);   // Or your preferred alternative
	    fprintf(stderr, "Failed to exec %s\n", cmd[0]);
	    exit(1);
	}
	else {
    // Parent process
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 1;
}
int yash_startprocess(char **args){
  pid_t pid;
  int status;
  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("yash: Child process successfully created. But cannot execvp() the command entered. Check Google for more help. Stackoverflow will do.");
    }
    exit(EXIT_FAILURE);
  } 
  else if (pid < 0) {
    perror("yash: Error creating Child process.");
  } 
  else {
    // Parent process
    insertintohistorypid(pid);
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 1;
}
int yash_execute(char **args,char* line){
  int i;
  if (args[0] == NULL) {
    return 1;
  }
  int n_builtin = sizeof(builtin_str) / sizeof(char *);
  for (i = 0; i < n_builtin; i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      insertintohistory(line);
      insertintohistorypid(getpid());
      return (*builtin_func[i])(args);
    }
  }
  insertintohistory(line);
  if(isalias(args[0]))
  	return 1;
  char* token;
  char* original = strdup(line);
  int len=strlen(line);
  token=strtok(line,"|");
  if(strlen(token)!=len)
  	return piping(original);
  token=strtok(line,"<>");
  if(strlen(token)!=len)
  	return ioredir(original);
  return yash_startprocess(args);
}
#define YASH_LINE_BUFSIZE 1024
char *yash_read_line(void){
  int bufsize = YASH_LINE_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    perror("yash: readline buffer cannot be allocated.");
    exit(EXIT_FAILURE);
  }
  while (1) {
    // Read a character
    c = getchar();
    if (c == EOF) {
      exit(EXIT_SUCCESS);
    }
    else if (c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } 
    else {
      buffer[position] = c;
    }
    position++;
    // If we have exceeded the buffer, reallocate.
    if (position == bufsize) {
      bufsize += YASH_LINE_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        perror("yash: readline buffer cannot be reallocated.");
        exit(EXIT_FAILURE);
      }
    }
  }
}
void yash_loop(void){
  char *line;
  char **args;
  int termstatus;
  do {
  	struct passwd* passwdstruct = getpwuid(getuid());
    printf("%s:%s$ ",passwdstruct->pw_name,cwd);
    line = yash_read_line();
    char* passline = strdup(line);
    args = yash_split_line(line);
    termstatus = yash_execute(args,passline);
    free(line);
    free(args);
  } while (termstatus);
}

int main(int argc, char **argv){
  getcwd(cwd, sizeof(cwd));
  yash_loop();
  return EXIT_SUCCESS;
}

