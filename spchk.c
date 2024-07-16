
/* Yash Patel and Waasil Ahmed
    ydp11           wa147
     Spelling Checker
*/

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_WORD_LENGTH 4096

typedef struct {
    char word[MAX_WORD_LENGTH];
} DictionaryWord;

DictionaryWord *dictionary = NULL;
size_t dictionarySize = 0;
bool spellingErrorReported = false; //To return EXIT_SUCCESS or EXIT_FAILURE

bool binarySearch(const char *word) {
    int low = 0, high = dictionarySize - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int res = strcmp(word, dictionary[mid].word);
        if (res == 0) return true;
        if (res > 0){
            low = mid + 1;
        }
        else{ high = mid - 1;
        }
    }
    return false;
}

bool capitalizationCheck(const char* textWord, const char* dictWord) {
    if (strcmp(textWord, dictWord) == 0) {
        return true;
    }

    //uppercase version
    char dictWordUpper[MAX_WORD_LENGTH];
    size_t len = strlen(dictWord);
    for (size_t i = 0; i < len; ++i) {
        dictWordUpper[i] = toupper((unsigned char)dictWord[i]);
    }
    dictWordUpper[len] = '\0';

    if (strcmp(textWord, dictWordUpper) == 0) {
        return true;
    }

    // Uppercase initial letter
    char dictWordInitialCapital[MAX_WORD_LENGTH];
    dictWordInitialCapital[0] = toupper((unsigned char)dictWord[0]);
    for (size_t i = 1; i < len; ++i) {
        dictWordInitialCapital[i] = tolower((unsigned char)dictWord[i]);
    }
    dictWordInitialCapital[len] = '\0';

    if (strcmp(textWord, dictWordInitialCapital) == 0) {
        return true;
    }

    return false;
}
bool checkVariations(const char* textword) {
    if (binarySearch(textword)){ 
        return true;
    }
    for (size_t i = 0; i < dictionarySize; ++i) {
        if (capitalizationCheck(textword, dictionary[i].word)) {
            return true;
        }
    }
    return false;
}

void reportError(const char *filePath, int line, int column, const char *word) {
    printf("%s (%d,%d): %s\n", filePath, line, column, word);
    spellingErrorReported = true;
}

void readDictionary(const char* dictionaryPath) {
    int fd = open(dictionaryPath, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open dictionary file");
        exit(EXIT_FAILURE);
    }

    size_t dictionaryCapacity = 100000;
    dictionary = (DictionaryWord*)malloc(dictionaryCapacity * sizeof(DictionaryWord));
    if (!dictionary) {
        perror("Failed to allocate memory for dictionary");
        close(fd);
        exit(EXIT_FAILURE);
    }

    char wordBuffer[MAX_WORD_LENGTH];
    ssize_t bytesRead;
    size_t wordIndex = 0, bufferIndex = 0;
    while ((bytesRead = read(fd, wordBuffer + bufferIndex, sizeof(wordBuffer) - bufferIndex - 1)) > 0) {
        bytesRead += bufferIndex; // Adjust for unused buffer space
        size_t start = 0;
        for (size_t i = 0; i < bytesRead; ++i) {
            if (wordBuffer[i] == '\n' || i == bytesRead - 1) { 
                wordBuffer[i] = '\0'; 
                // Resize dictionary 
                if (wordIndex == dictionaryCapacity) {
                    dictionaryCapacity *= 2; // Double the capacity
                    dictionary = (DictionaryWord*)realloc(dictionary, dictionaryCapacity * sizeof(DictionaryWord));
                    if (!dictionary) {
                        perror("Failed to resize dictionary");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }
                }
                strncpy(dictionary[wordIndex++].word, wordBuffer + start, MAX_WORD_LENGTH);
                start = i + 1;
            }
        }

        if (start < bytesRead) {
            bufferIndex = bytesRead - start;
            memmove(wordBuffer, wordBuffer + start, bufferIndex); 
        } else {
            bufferIndex = 0; 
        }
    }
    if (bytesRead < 0) {
        perror("Failed to read dictionary file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    dictionarySize = wordIndex; // Change size to actual size of dictionary
    close(fd);
}


void trimWord(char* word) {
    char* start = word;
    char* end = word + strlen(word) - 1;

    while (start <= end && !isalpha((unsigned char)*start)){
        start++;
    }

    while (end >= start && !isalpha((unsigned char)*end)){
        end--;
    }

    size_t length;
        if (end >= start) {
            length = (size_t)(end - start + 1);
        }
        else {
            length = 0;
        }

    if (word != start) {
        char *src = start;
        char *dest = word;
        while (*src != '\0') {
            *dest++ = *src++;
        }
        *dest = '\0';
    }

    word[length] = '\0';
}

bool processWordComponent(const char *component) {
    char mutableComponent[MAX_WORD_LENGTH];
    strncpy(mutableComponent, component, MAX_WORD_LENGTH);
    mutableComponent[MAX_WORD_LENGTH - 1] = '\0';

    trimWord(mutableComponent); 
    return checkVariations(mutableComponent);
}


bool processHyphenatedWord(const char *word) {
    char component[MAX_WORD_LENGTH];
    int componentIndex = 0;
    bool isValid = true;

    for (int i = 0; word[i] != '\0'; ++i) {
        if (word[i] == '-') {
            if (componentIndex > 0) { // End of a component
                component[componentIndex] = '\0'; 
                if (!processWordComponent(component)) isValid = false;
                componentIndex = 0; // Reset for the next component
            }
        } else {
            component[componentIndex++] = word[i]; // Add char to component
            if (componentIndex >= MAX_WORD_LENGTH - 1) {
                component[MAX_WORD_LENGTH - 1] = '\0'; // Safety null-termination
                isValid = false; // Component too long, automatically invalid
            }
        }
    }

    if (componentIndex > 0) {
        component[componentIndex] = '\0';
        if (!processWordComponent(component)) isValid = false;
    }

    return isValid;
}

void processBuffer(char *buffer, ssize_t length, const char *filePath, int *lineNum, int *colNum) {
    char word[MAX_WORD_LENGTH];
    int wordIndex = 0;
    bool inWord = false;

    for (int i = 0; i < length; ++i) {
        char ch = buffer[i];

        if (isalnum(ch) || (inWord && (ch == '-' || ch == '\''))) {
            word[wordIndex++] = ch;
            inWord = true;
            if (wordIndex >= MAX_WORD_LENGTH - 1) {
                word[MAX_WORD_LENGTH - 1] = '\0';
                reportError(filePath, *lineNum, *colNum - wordIndex + 1, word);
                wordIndex = 0;
                inWord = false;
            }
        } else {
            if (inWord) {
                word[wordIndex] = '\0';
                if (strchr(word, '-') != NULL) { // If the word is hyphenated
                    if (!processHyphenatedWord(word)) {
                        reportError(filePath, *lineNum, *colNum - wordIndex, word);
                    }
                } else { // If the word is not hyphenated
                    trimWord(word);
                    if (!checkVariations(word)) {
                        reportError(filePath, *lineNum, *colNum - wordIndex, word);
                    }
                }
                wordIndex = 0;
                inWord = false;
            }
        }

        if (ch == '\n') {
            (*lineNum)++;
            *colNum = 1;
        } else {
            (*colNum)++;
        }
    }


    if (inWord) {
        word[wordIndex] = '\0';
        if (strchr(word, '-') != NULL) {
            if (!processHyphenatedWord(word)) {
                reportError(filePath, *lineNum, *colNum - wordIndex, word);
            }
        } else {
            trimWord(word);
            if (!checkVariations(word)) {
                reportError(filePath, *lineNum, *colNum - wordIndex, word);
            }
        }
    }
}

void processTextFile(const char* filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }

    char buffer[4096];
    ssize_t bytesRead;
    int lineNum = 1, colNum = 1;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        processBuffer(buffer, bytesRead, filePath, &lineNum, &colNum);
    }

    if (bytesRead < 0) {
        perror("Failed to read file");
    }

    close(fd);
}



void traverseDirectory(const char* directoryPath) {
    DIR *dir = opendir(directoryPath);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.'){
            continue;
        }
        char fullPath[PATH_MAX];
        snprintf(fullPath, PATH_MAX, "%s/%s", directoryPath, entry->d_name);
        struct stat entryStat;
        if (stat(fullPath, &entryStat) == 0) {

            if (S_ISDIR(entryStat.st_mode)){
                traverseDirectory(fullPath);
            }
            else {
                char *dot = strrchr(entry->d_name, '.');
                if (dot && strcmp(dot, ".txt") == 0) processTextFile(fullPath);
            }

        }
        else perror("Failed to get file status");
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Wrong number of arguments\n");
        return EXIT_FAILURE;
    }
    readDictionary(argv[1]);
    struct stat pathStat;
    for (int i = 2; i < argc; ++i) {
        if (stat(argv[i], &pathStat) == 0) {
            if (S_ISDIR(pathStat.st_mode)){
                traverseDirectory(argv[i]);
            }
            else{
                processTextFile(argv[i]);
            }
        }
        else{
            perror("Error retrieving path");
        }
    }

    free(dictionary);
    if (spellingErrorReported) {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}