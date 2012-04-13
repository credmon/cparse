/*
    This is cparse, a C code parser.

    Copyright (C) 2012 Christopher Redmon.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#include <sys/ioctl.h>
#include <sys/types.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void);
static int open_file(const char* file);
static void parse_source_code(int fd, const char* file);
static void print_source_code(int fd, FILE* outfile, const char* file);

/****************************************************************/
/****************************************************************/
/* code tag format                                              */
/****************************************************************/
/****************************************************************/

static void add_tag(int line, int offset, const char* string);
static const char* find_tag(int line, int offset);
#if defined(DEBUG) && (DEBUG > 0)
static void dump_code_tags(void);
#endif

struct code_tag
{
   int line;
   int offset;
   const char* string;
   void* next;
};

static struct code_tag *code_tags_head = NULL;
static struct code_tag *code_tag = NULL;

/****************************************************************/
/****************************************************************/


/****************************************************************/
/****************************************************************/
/* syntax check format                                          */
/****************************************************************/
/****************************************************************/
enum syntax_type
{
  SYNTAX_TYPE_SINGLE_ENDED,
  SYNTAX_TYPE_DOUBLE_ENDED,
};

static void add_syntax_check(const char* syntax_check1,
                             const char* syntax_check2,
                             enum syntax_type syntax_type,
                             const char* open_string_if_found,
                             const char* close_string_if_found);
static void evaluate_syntax_checks(char* buffer, int buff_len, int line, int line_offset);
static int active_rule(void);
#if defined(DEBUG) && (DEBUG > 0)
static void dump_syntax(void);
#endif

struct syntax_check
{
   const char* syntax_check1;
   const char* syntax_check2;

   unsigned int syntax_check1_len;
   unsigned int syntax_check2_len;

   enum syntax_type syntax_type;

   const char* open_string_if_found;
   const char* close_string_if_found;

   // cannot be flush to another word
   unsigned int no_flush; //XXX

   unsigned int rule_active;
   unsigned int counter;

   void* next;
};

static struct syntax_check *syntax_list_head = NULL;
static struct syntax_check *syntax_list = NULL;

/****************************************************************/
/****************************************************************/

int main(int argc, char** argv)
{
   int opt, long_index;
  #if defined(DEBUG) && (DEBUG > 0)
   int code_tag_debug = 0;
  #endif
   char* file = NULL;
   FILE* outfile = NULL;
   struct option long_opts[] =
   {
      {
         name    : "help",
         has_arg : 0,
         flag    : NULL,
         val     : 'h',
      },
      {
         name    : "file",
         has_arg : 1,
         flag    : NULL,
         val     : 'f',
      },
      {
         name    : "output",
         has_arg : 1,
         flag    : NULL,
         val     : 'o',
      },
     #if defined(DEBUG) && (DEBUG > 0)
      {
         name    : "dump-syntax",
         has_arg : 0,
         flag    : NULL,
         val     : 's',
      },
     #endif
     #if defined(DEBUG) && (DEBUG > 0)
      {
         name    : "dump-tags",
         has_arg : 0,
         flag    : NULL,
         val     : 't',
      },
     #endif
   };

   /* preprocessor */
   add_syntax_check("#if", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=purple>","</font>");
   add_syntax_check("#elif", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=purple>","</font>");
   add_syntax_check("#else", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=purple>","</font>");
   add_syntax_check("#endif", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=purple>","</font>");
   add_syntax_check("#include", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=purple>","</font>");
   add_syntax_check("#define", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=purple>","</font>");

   /* control */
   add_syntax_check("while", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("for", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("switch", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("case", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("break", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("continue", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("do", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("if", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("else", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("default", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("sizeof", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");
   add_syntax_check("return", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#c0c000>","</font>");

   /* variables */
   add_syntax_check("void", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("unsigned", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("signed", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("int", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("const", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("char", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("static", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("long", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("float", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("double", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("short", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("struct", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");
   add_syntax_check("enum", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=#00ee00>","</font>");

   /* comments */
   add_syntax_check("/*", "*/", SYNTAX_TYPE_DOUBLE_ENDED, "<font color=#0000ff>","</font>");
   add_syntax_check("//", "\n", SYNTAX_TYPE_DOUBLE_ENDED, "<font color=#0000ff>","</font>");
   //add_syntax_check("#if 0", "#endif", SYNTAX_TYPE_DOUBLE_ENDED, "<font color=#0000ff>","</font>");

   /* strings */
   add_syntax_check("\"", "\"", SYNTAX_TYPE_DOUBLE_ENDED, "<font color=red>","</font>");
   add_syntax_check("'", "'", SYNTAX_TYPE_DOUBLE_ENDED, "<font color=red>","</font>");

   /* oddball */
   add_syntax_check("\\", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=red>","</font>");
   add_syntax_check("NULL", NULL, SYNTAX_TYPE_SINGLE_ENDED, "<font color=red>","</font>");


   while ((opt = getopt_long(argc, argv, "stf:ho:", long_opts, &long_index)) != -1)
   {
      switch (opt)
      {
         case 'h':
            usage();
            exit(0);
            break;
        #if defined(DEBUG) && (DEBUG > 0)
         case 's':
            dump_syntax();
            exit(0);
            break;
        #endif
        #if defined(DEBUG) && (DEBUG > 0)
         case 't':
            code_tag_debug = 1;
            break;
        #endif
         case 'f':
            file = strdup(optarg);
            break;
         case 'o':
            outfile = fopen(optarg, "w");
            if (outfile == NULL)
            {
               printf("error: could not open %s\n",optarg);
               exit(-1);
            }
            break;
         case ':':
         case '?':
            exit(-1);
            break;
         default:
            break;
      }
   }

   if (file == NULL)
   {
      usage();
      exit(-1);
   }

   if (outfile == NULL)
   {
      outfile = stdout;
   }

   {
      int fd;
      fd = open_file(file);

      /* parse file */
      parse_source_code(fd,file);

     #if defined(DEBUG) && (DEBUG > 0)
      if (code_tag_debug)
      {
         dump_code_tags();
      }
      else
     #endif
      {
         /* print source code */
         print_source_code(fd,outfile,file);
      }
      close(fd);
      fclose(outfile);
   }

   return 0;
}

static void usage(void)
{
   printf("Usage:\n");
   printf("  -h  --help ............................ print help\n");
   printf("  -f [file] --file [file] ............... specify file to parse\n");
   printf("  -o [file] --output [file] ............. specify output file\n");
  #if defined(DEBUG) && (DEBUG > 0)
   printf("\n");
   printf("Debug:\n");
   printf("  -s --dump-syntax ...................... dump syntax\n");
   printf("  -t --dump-tags ........................ dump tags\n");
  #endif
}

static int open_file(const char* file)
{
   int fd;

   fd = open(file,0);

   if (fd < 0)
   {
      printf("error: could not open %s\n",file);
      exit(-1);
   }

   return fd;
}

static void parse_source_code(int fd, const char* file)
{
   int buff_len = 1;
   char buffer[] = " ";
   int nc = 0;
   int line = 0;
   int line_offset = 0;

   lseek(fd, 0, SEEK_SET);

   while ((nc = read(fd, &buffer, buff_len)) > 0)
   {
      evaluate_syntax_checks(buffer, buff_len, line, line_offset);

      if (buffer[0] == '\n')
      {
         line++;
         line_offset = 0;
      }
      else
      {
         line_offset++;
      }
   }
}

static void print_source_code(int fd, FILE* outfile, const char* file)
{
   int buff_len = 1;
   char buffer[] = " ";
   int nc = 0;
   int line = 0;
   int line_offset = 0;
   int new_line = 1;

   fprintf(outfile, "<html>\n");
   fprintf(outfile, "<a name=\"%s\"></a><h3>%s</h3>\n",file,file);
   fprintf(outfile, "<pre>\n");

   lseek(fd, 0, SEEK_SET);

   while ((nc = read(fd, &buffer, buff_len)) > 0)
   {
      line_offset++;
      if (new_line)
      {
         if (active_rule()) { fprintf(outfile, "</font>"); fflush(outfile); }
         fprintf(outfile, "<a name=\"%s%d\"></a><font color=#000000>%06d</font> ",file,line,line);
         fflush(outfile);
         new_line = 0;
      }

      if (find_tag(line, line_offset))
      {
         fprintf(outfile, "%s",find_tag(line, line_offset));
         fflush(outfile);
      }

      if (buffer[0] == '\n')
      {
         new_line = 1;
         line++;
         line_offset = 0;
      }

      fprintf(outfile, "%s", buffer);
   }

   fprintf(outfile, "</pre>\n");
   fprintf(outfile, "</html>\n");
}

static void add_tag(int line, int offset, const char* string)
{
   if (code_tag == NULL)
   {
      code_tags_head = malloc(sizeof(struct code_tag));
      code_tag = code_tags_head;
   }
   else
   {
      code_tag->next = malloc(sizeof(struct code_tag));
      code_tag = code_tag->next;
   }

   code_tag->line   = line;
   code_tag->offset = offset;
   code_tag->string = strdup(string);
   code_tag->next = NULL;
}

static const char* find_tag(int line, int offset)
{
   struct code_tag *pTag;

   for (pTag = code_tags_head; pTag != NULL; pTag = pTag->next)
   {
      if (pTag->line < line)      { continue;    }
      if (pTag->line > line)      { return NULL; }
      if (pTag->offset > offset)  { return NULL; }
      if (pTag->offset >= offset) { break;       }

      if (pTag->next == NULL) return NULL;
   }

   if (pTag == NULL)
   {
      return NULL;
   }

   if ((pTag->line == line) && (pTag->offset == offset))
   {
      return pTag->string;
   }

   return NULL;
}

static void add_syntax_check(const char* syntax_check1,
                             const char* syntax_check2,
                             enum syntax_type syntax_type,
                             const char* open_string_if_found,
                             const char* close_string_if_found)
{
   if (syntax_list_head == NULL)
   {
      syntax_list_head = malloc(sizeof(struct syntax_check));
      syntax_list = syntax_list_head;
   }
   else
   {
      syntax_list->next = malloc(sizeof(struct syntax_check));
      syntax_list = syntax_list->next;
   }

   syntax_list->syntax_check1 = strdup(syntax_check1);
   syntax_list->syntax_check1_len = strlen(syntax_check1);

   if (syntax_check2 != NULL)
   {
      syntax_list->syntax_check2 = strdup(syntax_check2);
      syntax_list->syntax_check2_len = strlen(syntax_check2);
   }
   else
   {
      syntax_list->syntax_check2 = NULL;
      syntax_list->syntax_check2_len = 0;
   }
   syntax_list->syntax_type = syntax_type;
   syntax_list->open_string_if_found = strdup(open_string_if_found);
   syntax_list->close_string_if_found = strdup(close_string_if_found);
   syntax_list->rule_active = 0;
   syntax_list->no_flush = 1; // XXX
   syntax_list->counter = 0;
   syntax_list->next = NULL;
}

static int active_rule(void)
{
   struct syntax_check *pSyntax;

   for (pSyntax = syntax_list_head; pSyntax != NULL; pSyntax = pSyntax->next)
   {
      if (pSyntax->rule_active)
      {
         return 1;
      }
   }

   return 0;
}

static void evaluate_syntax_checks(char* buffer, int buff_len, int line, int line_offset)
{
   int i;
   struct syntax_check *pSyntax;
   static char previous_char = ' ';

   for (i = 0; i < buff_len; i++)
   {
      for (pSyntax = syntax_list_head; pSyntax != NULL; pSyntax = pSyntax->next)
      {
         if (active_rule())
         {
            if (pSyntax->rule_active)
            {
               if (pSyntax->syntax_check2[pSyntax->counter] == buffer[i])
               {
                  pSyntax->counter++;

                  if (pSyntax->counter == pSyntax->syntax_check2_len)
                  {
                     pSyntax->counter = 0;
                     add_tag(line, line_offset+2, pSyntax->close_string_if_found);
                     pSyntax->rule_active = 0;
                  }
               }
               else
               {
                  if (pSyntax->counter)
                  {
                     pSyntax->counter = 0;
                  }
               }
            }
            else
            {
               pSyntax->counter = 0;
            }
         }
         else
         {
            if ((pSyntax->syntax_check1[pSyntax->counter] == buffer[i])
               /*
                 &&
                ((pSyntax->no_flush == 0) ||
                 ((pSyntax->no_flush == 1) && (previous_char == ' ' || previous_char == ';' || previous_char == '(' || previous_char == '\n')))
               */
               )
            {
               pSyntax->counter++;

               if (pSyntax->counter == pSyntax->syntax_check1_len)
               {
                  pSyntax->counter = 0;
                  add_tag(line, line_offset+2-pSyntax->syntax_check1_len, pSyntax->open_string_if_found);
                  if (pSyntax->syntax_type == SYNTAX_TYPE_SINGLE_ENDED)
                  {
                     add_tag(line, line_offset+2, pSyntax->close_string_if_found);
                  }
                  else
                  {
                     pSyntax->rule_active = 1;
                  }
               }
            }
            else
            {
               if (pSyntax->counter)
               {
                  pSyntax->counter = 0;
               }
            }
         }

         //if (pSyntax->next == NULL) break;//return;
      }
      previous_char = buffer[i];
   }
}


#if defined(DEBUG) && (DEBUG > 0)
void dump_syntax(void)
{
   struct syntax_check *pSyntax;

   printf("%20s   %20s   %20s   %20s\n","Syntax Check #1","Syntax Check #2","Opening Tag","Closing Tag");
   printf("-----------------------------------------------------------------------------------------\n");
   for (pSyntax = syntax_list_head; pSyntax != NULL; pSyntax = pSyntax->next)
   {
      printf("%20s   %20s   %20s   %20s\n",pSyntax->syntax_check1,(pSyntax->syntax_check2 == NULL ? "NULL" :
                                                      ((strcmp(pSyntax->syntax_check2,"\n") == 0)) ? "nl"  : pSyntax->syntax_check2 ),
                             pSyntax->open_string_if_found, pSyntax->close_string_if_found);
   }
}
#endif

#if defined(DEBUG) && (DEBUG > 0)
static void dump_code_tags(void)
{
   struct code_tag *pTag;

   printf("%6s %6s : %s\n","line","offset","string");
   printf("------------------------------------\n");

   for (pTag = code_tags_head; pTag != NULL; pTag = pTag->next)
   {
      printf("%06d %06d : %s\n",pTag->line,pTag->offset,pTag->string);
   }
}
#endif
