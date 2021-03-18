#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include <dirent.h>

/*declarare variabile*/

int fd;
static char *args[512];
static char prompt[512];
char *history_file;
char *input_buffer;
char *cmd_exec[512];
int flag, len;
char cwd[1024];
pid_t pid;
int no_of_lines;
int  output_redirection, input_redirection;
char *input_redirection_file;
char *output_redirection_file;

/* declarare functii */

void clear_variables(); 
void print_history_list ();
void s_cd();
char *skipwhite (char* );
void tokenize_by_space (char *);
void tokenize_redirect_input(char *);
void tokenize_redirect_output(char *);
static int execute_inbuild_commands(char *, int, int, int);
void tokenize_by_pipe ();
void tokenize_by_chain ();
static int execute_command(int, int, int, char *);
void shell_prompt(); 
void s_mkdir();
void s_date();




/* Initializare variabile globale */

void clear_variables() 
{
	fd = 0;
	flag = 0;
	len = 0;
	no_of_lines = 0;
	output_redirection = 0;
	input_redirection = 0;
	cwd[0] = '\0';
	prompt[0] = '\0';
 	pid = 0;
}

/* history command function */

void print_history_list () 
{
  	register HIST_ENTRY **the_list;
    register int i;

    the_list = history_list ();
    if (the_list)
    	for (i = 0; the_list[i]; i++)
            printf ("%d: %s\n", i + history_base, the_list[i]->line);
    return;
}

/* fucntia pt data */

void s_date()
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    printf("now: %d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    return ;
}
  
/* Functia care creaza promptul (arata pathul in care ne aflam, si inca cateva detalii -> $ si Shell Project:) */

void shell_prompt() 
{
	if (getcwd(cwd, sizeof(cwd)) != NULL) 
	{
		strcpy(prompt, "Proiect shell> ");
		strcat(prompt, cwd);
		strcat(prompt, ":$ ");
	}
	else 
	{
		//eroare in cazul in care nu se poate afla path ul 
		perror("Error in getting curent working directory: ");
	}
	return;
}

/* functia care da skip la spatiile suplimentareS */

char *skipwhite (char* str) 
{
	int i = 0, j = 0;
	char *temp;
	if (NULL == (temp = (char *) malloc(sizeof(str)*sizeof(char)))) 
	{
		perror("Memory Error: ");
		return NULL;
	}

	while(str[i++]) 
	{
		if (str[i-1] != ' ')
			temp[j++] = str[i-1];
	}
	temp[j] = '\0';
	return temp;
}



/* cd command */

void s_cd() 
{
	char *home_dir = "/home";
	if ((args[1]==NULL) || (!(strcmp(args[1], "~") && strcmp(args[1], "~/"))))
		chdir(home_dir);
	else if (chdir(args[1]) < 0)
		perror("No such file or directory: ");

}

void s_mkdir()
{
    if(mkdir(args[1], 0777) == -1)
    {  
        int n;
        struct dirent** item_list;
        n = scandir(".", &item_list, NULL, alphasort);
        if(n < 0)
            perror("Error scandir");
        else
        {
            while(n--)
            {
            if(strcmp(item_list[n]->d_name,args[1])==0)
                {
                    printf("Exista deja un director cu acest nume.\n");
                    return ;
                }
            free(item_list[n]);
            }
        }
        free(item_list);
        
        perror("Nu se poate crea directorul.\n");
    }
    else
        printf("Director creat.\n");
    return ;
    
}
/* Aici se executa comenzile "inbuild" cele din cod (exit/quit,cd si history), 
daca comanda (args[0]) nu se afla in lista din cod, se va apela execute_command 
care va apela oarecum comanda din shell direct */

static int execute_inbuild_commands (char *cmd_exec, int input, int isfirst, int islast) 
{
	char *new_cmd_exec;
	new_cmd_exec = strdup(cmd_exec);
	tokenize_by_space (cmd_exec);

	if (args[0] != NULL) {
		if (!(strcmp(args[0], "exit") && strcmp(args[0], "quit")))
			exit(0);
		if (!strcmp("cd", args[0])) {

			s_cd();
			return 1;
		}
		if (!strcmp(args[0], "history")) 
		{
			print_history_list();
			return 1;
		}
		if (!strcmp(args[0], "mkdir")) 
		{
			s_mkdir();
			return 1;
		}
		if(!strcmp(args[0], "date"))
		{
			s_date();
			return 1;
		}

	}
	return (execute_command(input, isfirst, islast, new_cmd_exec));
}

/* imparte inputul in tokenuri cand se gaseste "<" in input */

void tokenize_redirect_input (char *cmd_exec) 
{
	char *val[128];
	char *new_cmd_exec, *s1;
	new_cmd_exec = strdup(cmd_exec);

	int m = 1;
	val[0] = strtok(new_cmd_exec, "<");
	while ((val[m] = strtok(NULL,"<")) != NULL) m++;

	s1 = strdup(val[1]);
	input_redirection_file = skipwhite(s1);

	tokenize_by_space (val[0]);
	return;
}

/* imparte inputul in tokenuri cand se gaseste ">" in input */
// nu prea merge acm

void tokenize_redirect_output (char *cmd_exec) 
{
	char *val[128];
	char *new_cmd_exec, *s1;
	new_cmd_exec = strdup(cmd_exec);

	int m = 1;
	val[0] = strtok(new_cmd_exec, ">");
	while ((val[m] = strtok(NULL,">")) != NULL) m++;

	s1 = strdup(val[1]);
	output_redirection_file = skipwhite(s1);

	tokenize_by_space (val[0]);
	return;
}


/* Functia asta creeaza pipe-ul si executa comenzile necodate direct din shell folosind execvp*/

static int execute_command (int input, int first, int last, char *cmd_exec) 
{
	int mypipefd[2], ret, input_fd, output_fd;

	if (-1 == (ret = pipe(mypipefd))) 
	{
		perror("pipe error: ");
		return 1;
	}

	pid = fork();

	if (pid == 0) 
	{

		if (first == 1 && last == 0 && input == 0) 
		{
			
			dup2 (mypipefd[1], 1);
		}
		else if (first == 0 && last == 0 && input != 0) 
		{
			dup2 (input, 0);
			dup2 (mypipefd[1], 1);
		}
		else 
		{
			dup2 (input, 0);
		}

		if (strchr(cmd_exec, '<')) 
		{
			input_redirection = 1;
			tokenize_redirect_input (cmd_exec);
		}
		else if (strchr(cmd_exec, '>')) 
		{
			output_redirection = 1;
			tokenize_redirect_output (cmd_exec);
		}
		

		if (output_redirection) 
		{
			if ((output_fd = creat(output_redirection_file, 0644)) < 0) 
			{
				fprintf(stderr, "Failed to open %s for writing\n", output_redirection_file);
				return (EXIT_FAILURE);
			}
			dup2 (output_fd, 1);
			close (output_fd);
			output_redirection = 0;
		}

		if (input_redirection) 
		{
			//nu prea merge 
			if ((input_fd = open(input_redirection_file, O_RDONLY, 0)) < 0) 
			{
				fprintf(stderr, "Failed to open %s for reading\n", input_redirection_file);
				return (EXIT_FAILURE);
			}
			dup2 (input_fd, 0);
			close (input_fd);
			input_redirection = 0;
		}
		
		if (execvp(args[0], args) < 0) 
		{
			fprintf(stderr, "%s: Command not found\n",args[0]);
		}
		exit(0);
	}

	else waitpid(pid,0,0);
	
	if (last == 1)
		close(mypipefd[0]);

	if (input != 0)
		close(input);

	close(mypipefd[1]);
	return (mypipefd[0]);
}

/* Imparte stringul in tokenuri dupa spatii  (" ") */

void tokenize_by_space (char *str) 
{
	int m = 1;

	args[0] = strtok(str, " ");
	while ((args[m] = strtok(NULL," ")) != NULL) m++;
	args[m] = NULL;
}

/* Functia asta imparte stringul dupa pipe ("|") */

void tokenize_by_pipe() 
{
	int i, n = 1, input = 0, first = 1;

	cmd_exec[0] = strtok(input_buffer, "|");
	while ((cmd_exec[n] = strtok(NULL, "|")) != NULL) n++;

	cmd_exec[n] = NULL;

	//se intra doar daca comanda contine un pipe
	for (i = 0; i < n-1; i++) 
	{
		input = execute_inbuild_commands(cmd_exec[i], input, first, 0);	
		first = 0;
	} 

	input = execute_inbuild_commands(cmd_exec[i], input, first, 1);
	return;
}

/* imparte stringul in tokene dupa chain "&&" */

void tokenize_by_chain() 
{
	int i, n = 1, input = 0, first = 1;

	cmd_exec[0] = strtok(input_buffer, "&&");
	while ((cmd_exec[n] = strtok(NULL, "&&")) != NULL) n++;

	cmd_exec[n] = NULL;

	//se intra doar daca comanda contine macar un chain
	for (i = 0; i < n-1; i++) 
	{
		input = execute_inbuild_commands(cmd_exec[i], input, first, 1);	
		first = 1;

	} 
	input = execute_inbuild_commands(cmd_exec[i], input, first, 0);

	return;
}

/*Main - ul */

int main() 
{
	system ("clear");
	int working=1;
	//pt upparrow downarrow
	using_history();

	do 
	{
		clear_variables();
		shell_prompt();
		input_buffer = readline (prompt);

		
		if(strcmp(input_buffer,"\n"))
			add_history (input_buffer);
		//daca se apasa enter - sa nu apare eraore : "Segmentation fault"
		if (!(strcmp(input_buffer, "\n") && strcmp(input_buffer,"")))
			continue;

		if (!(strncmp(input_buffer, "exit", 4) && strncmp(input_buffer, "quit", 4))) 
		{
			working = 0;
			break;
		}
		
		if(strstr(input_buffer,"&&"))
			tokenize_by_chain();
		else 
			tokenize_by_pipe();


	} while(working);

	if (working == 0) 
	{
		printf("\nSe inchide shellul.\n");
		exit(0);
	}

	return 0;
}