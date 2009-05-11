#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "arg.h"
#include "chplcast.h"
#include "chplcgfns.h"
#include "chplexit.h"
#include "chplio.h"
#include "chplmem.h"
#include "chplmemtrack.h"
#include "chplrt.h"
#include "chplthreads.h"
#include "config.h"
#include "error.h"
#include "gdb.h"


static int gdbFlag = 0;
int32_t blockreport = 0; // report locations of blocked threads on SIGINT
int32_t taskreport = 0;  // report thread hierarchy on SIGINT

int _runInGDB(void) {
  return gdbFlag;
}

static void printHeaders(char thisType, char* lastType) {
  if (thisType != *lastType) {
    fprintf(stdout, "\n");
    switch (thisType) {
    case 'c':
      fprintf(stdout, "CONFIG VAR FLAGS:\n");
      fprintf(stdout, "=================\n");
      break;

    case 'g':
      fprintf(stdout, "FLAGS:\n");
      fprintf(stdout, "======\n");
      break;
      
    case 'm':
      fprintf(stdout, "MEMORY FLAGS:\n");
      fprintf(stdout, "=============\n");        
      break;
    }
    *lastType = thisType;
  }
}


static void printHelpTable(void) {
  typedef struct _flagType {
    const char* flag;
    const char* description;
    const char headerType;
  } flagType;

  static flagType flagList[] = {
    {"-h, --help", "print this message", 'g'},
    {"-nl <n>", "run program using n locales", 'g'},
    {"", "(equivalent to setting the numLocales config const)", 'g'},
    {"-q, --quiet", "run program in quiet mode", 'g'},
    {"-v, --verbose", "run program in verbose mode", 'g'},
    {"-b, --blockreport", "report location of blocked threads on SIGINT", 'g'},
    {"-t, --taskreport",
     "report list of pending and executing tasks on SIGINT", 'g'},
    {"--gdb", "run program in gdb", 'g'},

    {"-s, --<cfgVar>=<val>", "set the value of a config var", 'c'},    
    {"-f<filename>", "read in a file of config var assignments", 'c'},

    {"--memmax=<n>", "simulate 'n' bytes of memory available", 'm'}, 
    {"--memstat", "print memory statistics", 'm'},
    {"--memtrack", "track dynamic memory usage using a table", 'm'},
    {"--memtrace=<filename>", "write memory trace to filename", 'm'},
    {"--memthreshold=<n>", "filter memtrace for sizes >= 'n' bytes", 'm'},

    {NULL, NULL, ' '}
  };

  int i = 0;
  int longestFlag = 0;
  char lastHeaderType = '\0';

  while (flagList[i].flag) {
    int thisFlag = strlen(flagList[i].flag);
    if (longestFlag < thisFlag) {
      longestFlag = thisFlag;
    }
    i++;
  }

  i = 0;
  while (flagList[i].flag) {
    printHeaders(flagList[i].headerType, &lastHeaderType);
    if (flagList[i].flag[0] == '\0') {
      fprintf(stdout, "  %-*s    %s\n", longestFlag, flagList[i].flag,
              flagList[i].description);
    } else {
      fprintf(stdout, "  %-*s  : %s\n", longestFlag, flagList[i].flag, 
              flagList[i].description);
    }
    i++;
  }
  fprintf(stdout, "\n");
}


static int64_t getIntArg(char* valueString, const char* memFlag, 
                             int32_t lineno, chpl_string filename) {
  char extraChars;
  int64_t value = 0;  /* initialization is silly hack for freebsd */
  int numScans;
  char* message;

  if (!valueString || strcmp(valueString, "") == 0) {
    message = chpl_glom_strings(3, "The ", memFlag, " flag is missing its int input");
    chpl_error(message, lineno, filename);
  }
  numScans = sscanf(valueString, _default_format_read_int64"%c", 
                    &value, &extraChars);
  if (numScans != 1) {
    message = chpl_glom_strings(2, valueString, " is not of int type");
    chpl_error(message, lineno, filename);
  }
  return value; 
}


static char* getStringArg(char* valueString, const char* memFlag, 
                             int32_t lineno, chpl_string filename) {
  char* message;
  if (!valueString || strcmp(valueString, "") == 0) {
    message = chpl_glom_strings(3, "The ", memFlag, " flag is missing its string input");
    chpl_error(message, lineno, filename);
  }
  return valueString;
}


static void exitIfEqualsSign(char* equalsSign, const char* memFlag, 
                             int32_t lineno, chpl_string filename) {
  if (equalsSign) {
    char* message = chpl_glom_strings(3, "The ", memFlag, " flag takes no argument");
    chpl_error(message, lineno, filename);
  }
}

typedef enum _MemFlagType {MemMax, MemStat, MemFinalStat, MemTrack, MemThreshold, MemTrace,
                           MOther} MemFlagType;

static int parseMemFlag(const char* memFlag, int32_t lineno, chpl_string filename) {
  char* equalsSign;
  char* valueString;
  MemFlagType flag = MOther;
  if (strncmp(memFlag, "memmax", 6) == 0) {
    flag = MemMax;
  } else if (strncmp(memFlag, "memstat", 7) == 0) {
    flag = MemStat;
  } else if (strncmp(memFlag, "memfinalstat", 12) == 0) {
    flag = MemFinalStat;
  } else if (strncmp(memFlag, "memtrack", 8) == 0) {
    flag = MemTrack;
  } else if (strncmp(memFlag, "memthreshold", 12) == 0) {
    flag = MemThreshold;
  } else if (strncmp(memFlag, "memtrace", 8) == 0) {
    flag = MemTrace;
  }

  if (flag == MOther) {
    return 0;
  }

  equalsSign = strchr(memFlag, '=');
  valueString = NULL;

  if (equalsSign) {
    valueString = equalsSign + 1;
  }

  switch (flag) {
  case MemMax:
    {
      int64_t value;
      value = getIntArg(valueString, "--memmax", lineno, filename);
      setMemmax(value);
      break;
    }

  case MemStat:
    {
      exitIfEqualsSign(equalsSign, "--memstat", lineno, filename);
      setMemstat();
      break;
    }
   
  case MemFinalStat:
    {
      exitIfEqualsSign(equalsSign, "--memfinalstat", lineno, filename);
      setMemfinalstat();
      break;
    }
   
  case MemTrack:
    {
      exitIfEqualsSign(equalsSign, "--memtrack", lineno, filename);
      setMemtrack();
      break;
    }

  case MemThreshold:
    {
      int64_t value;
      value = getIntArg(valueString, "--memthreshold", lineno, filename);
      setMemthreshold(value);
      break;
    }

  case MemTrace:
    {
      valueString = getStringArg(valueString, "--memtrace", lineno, filename);
      setMemtrace(valueString);
      break;
    }

  case MOther:
    return 0;  // should never actually get here
  }

  return 1;
}


static int32_t _argNumLocales = 0;

void parseNumLocales(const char* numPtr, int32_t lineno, chpl_string filename) {
  int invalid;
  char invalidChars[2] = "\0\0";
  _argNumLocales = chpl_string_to_int32_t_precise(numPtr, &invalid, invalidChars);
  if (invalid) {
    char* message = chpl_glom_strings(3, "\"", numPtr, 
                                      "\" is not a valid number of locales");
    chpl_error(message, lineno, filename);
  }
  if (_argNumLocales < 1) {
    chpl_error("Number of locales must be greater than 0", lineno, filename);
  }
}


int32_t getArgNumLocales(void) {
  return _argNumLocales;
}


void parseArgs(int* argc, char* argv[]) {
  int i;
  int printHelp = 0;
  int origargc = *argc;

  for (i = 1; i < *argc; i++) {
    const char* filename = "<command-line arg>";
    int lineno = i + (origargc - *argc);
    int argLength = 0;
    const char* currentArg = argv[i];
    argLength = strlen(currentArg);

    if (argLength < 2) {
      const char* message = chpl_glom_strings(3, "\"", currentArg, 
                                              "\" is not a valid argument");
      chpl_error(message, lineno, filename);
    }

    switch (currentArg[0]) {
    case '-':
      switch (currentArg[1]) {
      case '-':
        {
          const char* flag = currentArg + 2;

          if (strcmp(flag, "gdb") == 0) {
            gdbFlag = i;
            break;
          }
            
          if (strcmp(flag, "help") == 0) {
            printHelp = 1;
            break;
          }
          if (strcmp(flag, "verbose") == 0) {
            verbosity=2;
            break;
          }
          if (strcmp(flag, "blockreport") == 0) {
            blockreport = 1;
            break;
          }
          if (strcmp(flag, "taskreport") == 0) {
            taskreport = 1;
            break;
          }
          if (strcmp(flag, "quiet") == 0) {
            verbosity = 0;
            break;
          }
          if (flag[0] == 'm' && parseMemFlag(flag, lineno, filename)) {
            break;
          }
          if (argLength < 3) {
            char* message = chpl_glom_strings(3, "\"", currentArg, 
                                              "\" is not a valid argument");
            chpl_error(message, lineno, filename);
          }
          i += handlePossibleConfigVar(argc, argv, i, lineno, filename);
          break;
        }

      case 'f':
        if (currentArg[2] == '\0') {
          i++;
          if (i >= *argc) {
            chpl_error("-f flag is missing <filename> argument", 
                       lineno, filename);
          }
          currentArg = argv[i];
          parseConfigFile(currentArg, lineno, filename);
        } else {
          parseConfigFile(currentArg + 2, lineno, filename);
          break;
        }

      case 'h':
        if (currentArg[2] == '\0') {
          printHelp = 1;
        } else {
          i += handleNonstandardArg(argc, argv, i, lineno, filename);
        }
        break;

      case 'n':
        if (currentArg[2] == 'l') {
          const char* numPtr;
          if (currentArg[3] == '\0') {
            i++;
            if (i >= *argc) {
              chpl_error("-nl flag is missing <numLocales> argument", 
                         lineno, filename);
            }
            currentArg = argv[i];
            numPtr = currentArg;
          } else {
            numPtr = &(currentArg[3]);
          }
          initSetValue("numLocales", numPtr, "Built-in", lineno, filename);
          break;
        }
        i += handleNonstandardArg(argc, argv, i, lineno, filename);
        break;

      case 'q':
        if (currentArg[2] == '\0') {
          verbosity = 0;
        } else {
          i += handleNonstandardArg(argc, argv, i, lineno, filename);
        }
        break;

      case 's':
        {
          if (argLength < 3) {
            char* message = chpl_glom_strings(3, "\"", currentArg, 
                                              "\" is not a valid argument");
            chpl_error(message, lineno, filename);
          }
          i += handlePossibleConfigVar(argc, argv, i, lineno, filename);
          break;
        }

      case 'v':
        if (currentArg[2] == '\0') {
          verbosity = 2;
        } else {
          i += handleNonstandardArg(argc, argv, i, lineno, filename);
        }
        break;
      case 'b':
        if (currentArg[2] == '\0') {
          blockreport = 1;
        } else {
          i += handleNonstandardArg(argc, argv, i, lineno, filename);
        }
        break;
      case 't':
        if (currentArg[2] == '\0') {
            taskreport = 1;
        } else {
          i += handleNonstandardArg(argc, argv, i, lineno, filename);
        }
        break;
      default:
        i += handleNonstandardArg(argc, argv, i, lineno, filename);
      }
      break;

    default:
      i += handleNonstandardArg(argc, argv, i, lineno, filename);
    }
  }

  if (printHelp) {
    printHelpTable();
    printConfigVarTable();
  }
}
