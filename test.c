#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void test(const char *path){
    printf("Called test on path %s\n", path);
    
    char* truncatedPath = path+1;
    printf("Truncated: %s\n", truncatedPath);

    int i=0;
	int index = -1;
	for (i = 0; i < strlen(truncatedPath); i++){
        printf("character: %c\n", truncatedPath[i]);
        if (truncatedPath[i] == '/'){
			index = i;
            printf("Found / at index %d\n", index);
			break;
		}
	}	
    if (index == -1){
        printf("error: no / in truncated path\n");
		return; //no / found in truncatedpath
    }

	

	// /foo/bar/a.txt 
	// /bar/a.txt 
	// /a.txt
	// char[1000] = dir_name
	//memset dir_name to '\0'
	char* directory_name = malloc(index);
	memcpy(directory_name, truncatedPath, index);
    printf("resulting directory name: %s\n", directory_name);
    char* substring = strstr(truncatedPath, "/");
    printf("next path to pass in is: %s", substring);
    test(substring);
    
}

int main(void) {
    const char *path = "/directory1/directory2/directory3/directory4/file1.txt";
    test(path);


    return 0;
}