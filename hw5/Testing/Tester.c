#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define ERR(s) err(s, __FILE__, __LINE__)

static void err(char *s, char *file, int line)
{
    fprintf(stderr, "%s:%d: %s\n", file, line, s);
    exit(1);
}

// Prints usage statement 
static void printUsage(int scanner)
{
    printf("%s", "Usage:");
    printf("%s", "\n<./Tester DEFAULT>");
    printf("%s", "\n<./Tester sepCharacters separatorCount>\n");
    close(scanner);
    exit(-1);
}

int main(int argc, char *argv[])
{
    enum {max = 100};
    int sepCount;
    
    // First argument is the separators
    char *separg = argv[1];

    // buffer and length
    char buf[max + 1];
    int len;

    int scanner = open("/dev/Scanner", O_RDWR);

    // Makes sure at least two args were given 
    if (argc < 2)
    {
        printUsage(scanner);
    }
    else
    {
        // Checks if the second argument is default
        if ((strcmp(separg, "DEFAULT")))
        {
            // If not default we will need another argument, (length of separators)
            if (argc < 3)
            {
                printUsage(scanner);
            }
            // Converts the string to integer
            sepCount = atoi(argv[2]);
            
            // Writes to ioctl if necessary
            if (ioctl(scanner, 0, 0))
                ERR("ioctl() for separators failed");
            if (write(scanner, separg, sepCount) != sepCount)
                ERR("write() of separators failed");
        }

        if (scanner < 0)
            ERR("open() failed");

        // Read and write loop to device driver
        char *line;
        while (scanf("%m[^\n]\n", &line) != EOF)
        {
            len = strlen(line);
            if (len != write(scanner, line, len))
                ERR("write() of data failed");
            free(line);
            while ((len = read(scanner, buf, max)) >= 0)
            {
                buf[len] = 0;
                printf("%s%s", buf, (len ? "" : "\n"));
            }
        }

        close(scanner);
        return 0;
    }
}