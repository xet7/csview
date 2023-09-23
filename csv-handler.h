#ifndef csvhandler_h
#define csvhandler_h

// Constants

#define CSV_HANDLER__OK                     0
#define CSV_HANDLER__FILE_NOT_FOUND         1
#define CSV_HANDLER__EOF                    2
#define CSV_HANDLER__LINE_IS_NULL           3

char csv_handler_read_next_line();

char csv_handler_line(char **wholeLine);

char csv_handler_output_line(char **outputLine);

#endif