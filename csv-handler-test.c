#include <stdlib.h>
#include <stdio.h>

#include "csv-handler.h"

void testfunc(char **line);

int main()
{
    char *outputLine = NULL;
    char *borderLine = NULL;

    // Print header, with border.
    csv_handler_read_next_line(); // Not bothering checking RCs right now.
    csv_handler_border_line(&borderLine);
    printf("%s\n", borderLine);
    csv_handler_output_line(&outputLine);
    printf("%s\n", outputLine);
    printf("%s\n", borderLine);

    while (csv_handler_read_next_line() == CSV_HANDLER__OK) {
        csv_handler_output_line(&outputLine);
        printf("%s\n", outputLine);
    }

    printf("%s\n", borderLine);

    free(outputLine);
    free(borderLine);
    csv_handler_close();
}
