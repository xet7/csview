#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"
#include "csv-handler.h"

/**
 * Delimiter.
 */
static char delim = ',';

/**
 * Current complete line.
 */
static char *line = NULL;

/**
 * Headers as array of strings.
 */
static char **headers = NULL;

/**
 * The fields to be included in output.  Terminated by -1.  If NULL, then
 * include all values.
 */
static int *selectedFields = NULL;

/**
 * Count of headers in source file.
 */
static int countHeaders = -1;

/**
 * Count of fields displayed in output.  -1 means display everything.
 */
static int selectedFieldCount = -1;

/**
 * Width of cells to output.
 */
static int width = 15;

/**
 * Array of array of strings, terminated by a NULL at the end.  Good grief;
 * this will be annoying.  Holds the entirety of the input file except what is
 * filtered out.  Currently only used for displaying transposed output.
 */
static char ***entireInput = NULL;


// START forward declarations for static functions.

static char getParsedLine(char ***parsedLine);

static char appendBoxedValue(char **outputLine, char *newValue);

static int getSelectedFieldCount();

static char setFieldsForTransposed(char *fields);

static char *getHeaderFromPosition(int pos);

static char unparseValue(char **value);

// END forward declarations.

/**
 * Read next line into memory.
 */
char csv_handler_read_next_line()
{
    if (line != NULL) {
        free(line);
        line = NULL;
    }

    line = (char *) malloc(sizeof(char));

    if (line == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    line[0] = '\0'; // Empty string for now because we don't know how long it
    // will be.

    int buffsize = 255;
    char buff[buffsize];
    while (1) {
        if (fgets(buff, buffsize, stdin) == NULL) {
            // Note that this should happen *after* the final line has already
            // been read into memory.
            return CSV_HANDLER__DONE;
        }

        line = (char *) realloc(
            line,
            sizeof(char) * (strlen(line) + strlen(buff) + 1) // +1 for null terminator
        );

        if (line == NULL) {
            return CSV_HANDLER__OUT_OF_MEMORY;
        }

        strcat(line, buff);

        size_t lst = strlen(line) - 1;
        if (line[lst] == '\n' && ((countHeaders = count_fields(line, delim)) != -1)) {
            // If count_fields is -1, then that means the line is not parseable
            // as a CSV line, which probably means that the file has a field
            // with a line break in it, meaning we have to include both of the
            // *file*'s lines as part of the same logical CSV line.  Example:

            // field one,field two,"field with
            // line break", field four

            // From the CSV perspective, this is one line, but if we don't check
            // that the line we just found is parseable when we reach the first
            // newline, we'll get an unparseable string and csv.c will return
            // null.

            line[lst] = '\0'; // Removing newline, but not reallocing.
            break;
        }
    }

    return CSV_HANDLER__OK;
}

/**
 * Set the headers from the line in memory.
 */
char csv_handler_set_headers_from_line()
{
    if (line == NULL) {
        return CSV_HANDLER__LINE_IS_NULL;
    }
    if (headers != NULL) {
        return CSV_HANDLER__ALREADY_SET;
    }

    headers = parse_csv(line, delim);
    // Not using getParsedLine because don't want to filter anything out for
    // headers.

    return CSV_HANDLER__OK;
}

/**
 * Get a single header.
 *
 * @param   outputLine
 */
char csv_handler_output_headers(char **outputLine)
{
    static int ind = -1;

    if (*outputLine != NULL) {
        free(*outputLine);
        *outputLine = NULL;
    }

    ind++;

    if (headers[ind] == NULL) {
        return CSV_HANDLER__DONE;
    }

    *outputLine = malloc(sizeof(char) * (strlen(headers[ind]) + 1));
    strcpy(*outputLine, headers[ind]);

    return CSV_HANDLER__OK;
}

/**
 * Get the line in CSV format.
 *
 * @param   wholeLine   Pointer to string.
 */
char csv_handler_raw_line(char **wholeLine)
{
    if (line == NULL) {
        return CSV_HANDLER__LINE_IS_NULL;
    }

    if (*wholeLine != NULL) {
        free(*wholeLine);
        *wholeLine = NULL;
    }

    char **parsedLine = NULL;
    getParsedLine(&parsedLine);

    *wholeLine = malloc(sizeof(char));

    if (*wholeLine == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    strcpy(*wholeLine, "");

    char rc = 0;
    for (int i = 0; parsedLine[i] != NULL; i++) {
        if ((rc = unparseValue(&(parsedLine[i]))) != CSV_HANDLER__OK) {
            return rc;
        }
        *wholeLine = realloc(
            *wholeLine,
            sizeof(char) * (strlen(*wholeLine) + strlen(parsedLine[i]) + 2)
        );
        if (*wholeLine == NULL) {
            return CSV_HANDLER__OUT_OF_MEMORY;
        }
        strcat(*wholeLine, parsedLine[i]);
        strcat(*wholeLine, ",");
    }

    (*wholeLine)[strlen(*wholeLine) - 1] = '\0';  // Don't resize.
    free_csv_line(parsedLine);

    return CSV_HANDLER__OK;
}

/**
 * Get line to print out to stdout.
 *
 * @param   outputLine
 */
char csv_handler_output_line(char **outputLine)
{
    if (*outputLine != NULL) {
        free(*outputLine);
        *outputLine = NULL;
    }

    if (line == NULL) {
        return CSV_HANDLER__LINE_IS_NULL;
    }
    char **parsedLine = NULL;
    getParsedLine(&parsedLine);

    *outputLine = malloc(sizeof(char) * 2);

    if (*outputLine == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    (*outputLine)[0] = '|'; // Opening brace.
    (*outputLine)[1] = '\0';

    char rc = 0;
    for (int i = 0; parsedLine[i] != NULL; i++) {
        if ((rc = appendBoxedValue(outputLine, parsedLine[i])) != CSV_HANDLER__OK) {
            return rc;
        }
    }

    free_csv_line(parsedLine);

    return CSV_HANDLER__OK;
}

/**
 * Get border to print out.
 *
 * @param   outputLine
 */
char csv_handler_border_line(char **outputLine)
{
    if (countHeaders == -1) {
        return CSV_HANDLER__LINE_IS_NULL; // Not sure what else to call this.
    }

    if (*outputLine != NULL) {
        free(*outputLine);
        *outputLine = NULL;
    }

    int lineLen = (width + 1) * getSelectedFieldCount() + 1;
    // I almost wanted to name this "linLen" because then it would be pronounced
    // "len-len" and that would be funny.
    // (width + 1) is the width of every field plus its left brace.
    // + 2 is one for rightmost brace.
    // Null terminator is *not* included here, because want to match what the
    // result of strlen would be.

    *outputLine = (char *) malloc(sizeof(char) * (lineLen + 1));

    if (*outputLine == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    (*outputLine)[0] = '+';

    for (int i = 1; i < lineLen - 1; i++) {
        (*outputLine)[i] = '-';
    }
    (*outputLine)[lineLen - 1] = '+';
    (*outputLine)[lineLen] = '\0';

    return CSV_HANDLER__OK;
}

/**
 * Get *vertical* entry to print to stdout.
 *
 * This is not a single line, so behavior is inconsistent.  It's the entirety of
 * an entry, line breaks and all.
 *
 * @param   outputEntry
 */
char csv_handler_output_vertical_entry(char **outputEntry)
{
    if (*outputEntry != NULL) {
        free(*outputEntry);
        *outputEntry = NULL;
    }

    if (line == NULL) {
        return CSV_HANDLER__LINE_IS_NULL;
    }

    if (headers == NULL) {
        return CSV_HANDLER__HEADERS_NOT_SET;
    }

    char **parsedLine = NULL;
    getParsedLine(&parsedLine);

    *outputEntry = malloc(sizeof(char));
    (*outputEntry)[0] = '\0';

    for (int i = 0; parsedLine[i] != NULL; i++) {
        *outputEntry = realloc(
            *outputEntry,
            strlen(*outputEntry)
            + strlen(parsedLine[i])
            + strlen(headers[i])
            + (i == 0 ? 3 : 4)
        );
        // +1 for null term, +1 for line break, +2 for ": "
        if (i != 0) {
            strcat(*outputEntry, "\n");
        }
        strcat(*outputEntry, getHeaderFromPosition(i));
        strcat(*outputEntry, ": ");
        strcat(*outputEntry, parsedLine[i]);
    }

    free_csv_line(parsedLine);

    return CSV_HANDLER__OK;
}

/**
 * Get border line for vertical entry.
 *
 * @param   outputLine
 */
char csv_handler_vertical_border_line(char **outputLine)
{
    if (*outputLine != NULL) {
        free(*outputLine);
        *outputLine = NULL;
    }

    *outputLine = malloc(sizeof(char) * width + 1); // Re-use width, so can
    // change it if want to.

    if (*outputLine == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    for (int i = 0; i < width; i++) {
        (*outputLine)[i] = '*';
    }

    (*outputLine)[width] = '\0';

    return CSV_HANDLER__OK;
}

/**
 * Read the entirety of the file (that's desired) into memory so that can output
 * it as transposed.
 *
 * @param   fields      Passed fields, if any.
 */
char csv_handler_initialize_transpose(char *fields)
{
    if (entireInput != NULL) {
        return CSV_HANDLER__ALREADY_SET;
    }

    char **parsedLine = NULL;
    int arrLen = 0;
    char rc = 0;

    while (csv_handler_read_next_line() == CSV_HANDLER__OK) {
        if ((rc = setFieldsForTransposed(fields)) != CSV_HANDLER__OK) {
            return rc;
        }

        getParsedLine(&parsedLine);
        arrLen++;
        entireInput = realloc(entireInput, sizeof(char ***) * arrLen);

        if (entireInput == NULL) {
            return CSV_HANDLER__OUT_OF_MEMORY;
        }

        entireInput[arrLen - 1] = (char **) malloc(sizeof(char **) * (countHeaders + 1));

        if (entireInput[arrLen - 1] == NULL) {
            return CSV_HANDLER__OUT_OF_MEMORY;
        }

        int i = 0;
        for (; parsedLine[i] != NULL; i++) {
            entireInput[arrLen - 1][i] = (char *) malloc(sizeof(char) * strlen(parsedLine[i]) + 1);

            if (entireInput[arrLen - 1] == NULL) {
                return CSV_HANDLER__OUT_OF_MEMORY;
            }

            strcpy(entireInput[arrLen - 1][i], parsedLine[i]);
        }

        entireInput[arrLen - 1][i] = NULL;

        free_csv_line(parsedLine);
    }

    entireInput = (char ***) realloc(entireInput, sizeof(char **) * (arrLen + 1));

    if (entireInput == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    entireInput[arrLen] = NULL;

    return CSV_HANDLER__OK;
}

/**
 * Get transposed line to print to stdout.
 *
 * @param   outputLine
 */
char csv_handler_transposed_line(char **outputLine)
{
    // This one's very different.  Everything is already stored in memory in
    // entireInput, so go down the line from there.

    static int ind = 0;

    if (*outputLine != NULL) {
        free(*outputLine);
        *outputLine = NULL;
    }

    if (entireInput == NULL) {
        return CSV_HANDLER__LINE_IS_NULL;
    }

    if (entireInput[0][ind] == NULL) {
        return CSV_HANDLER__DONE;
    }

    *outputLine = (char *) malloc(sizeof(char) * 2);

    if (*outputLine == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    strcpy(*outputLine, "[");

    char rc = 0;
    for (int i = 0; entireInput[i] != NULL; i++) {
        if ((rc = appendBoxedValue(outputLine, entireInput[i][ind])) != CSV_HANDLER__OK) {
            return rc;
        }

        if (i == 0) {
            int j = strlen(*outputLine) - 3;
            for (; (*outputLine)[j] == ' ' && j > 0;j--) {}
            (*outputLine)[j + 1] = ']';
        }
    }

    ind++;

    return CSV_HANDLER__OK;
}

/**
 * Get transposed border line to print to stdout.
 *
 * @param   outputLine
 */
char csv_handler_transposed_border_line(char **outputLine)
{
    if (*outputLine != NULL) {
        free(*outputLine);
        *outputLine = NULL;
    }

    if (entireInput == NULL) {
        return CSV_HANDLER__LINE_IS_NULL;
    }

    int len = 0;

    // Count up the number of fields in the array.
    for (;entireInput[++len] != NULL;){};

    len = (len * (width + 1)) + 1;
    // len is number of elements in first row, so multiply it by field width
    // (plus one for |), plus one at the end for final +

    *outputLine = (char *) malloc(sizeof(char) * (len + 1));

    if (*outputLine == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    for (int i = 0; i < len - 1; i++) {
        (*outputLine)[i] = '-';
    }

    (*outputLine)[len - 1] = '+';
    (*outputLine)[len] = '\0';

    return CSV_HANDLER__OK;
}

/**
 * Change the width.
 *
 * @param   newWidth
 */
void csv_handler_set_width(int newWidth)
{
    width = newWidth;
}

/**
 * Set the selected fields.  (String input will be same as CSV format.)
 *
 * @param   fields
 */
char csv_handler_set_selected_fields(char *fields)
{
    if (headers == NULL) {
        return CSV_HANDLER__HEADERS_NOT_SET;
    }
    if (selectedFields != NULL) {
        return CSV_HANDLER__ALREADY_SET;
    }

    selectedFieldCount = count_fields(fields, delim);
    selectedFields = malloc(sizeof(int) * (getSelectedFieldCount() + 1));
    char **fieldArr = parse_csv(fields, delim);

    if (fieldArr == NULL) {
        return CSV_HANDLER__INVALID_INPUT;
    }

    for (int i = 0; fieldArr[i] != NULL; i++) {
        selectedFields[i] = -1;
        for (int j = 0; headers[j] != NULL; j++) {
            if (strcmp(fieldArr[i], headers[j]) == 0) {
                selectedFields[i] = j;
            }
        }
        if (selectedFields[i] == -1) {
            // Nothing was found, so return rc.
            return CSV_HANDLER__HEADER_NOT_FOUND;
        }
    }

    selectedFields[getSelectedFieldCount()] = -1;

    free_csv_line(fieldArr);

    return CSV_HANDLER__OK;
}

/**
 * Close out everything.
 */
char csv_handler_close()
{
    if (headers != NULL) {
        free_csv_line(headers);
    }
    if (entireInput != NULL) {
        for (int i = 0; entireInput[i] != NULL; i++) {
            free_csv_line(entireInput[i]);
            entireInput[i] = NULL;
        }
        free(entireInput);
    }

    free(line);
    line = NULL;
    free(selectedFields);
    selectedFields = NULL;

    return CSV_HANDLER__OK;
}


// Static functions below this line.

/**
 * Set passed pointer to array of strings to parsed CSV from line.
 *
 * @param   parsedLine
 */
static char getParsedLine(char ***parsedLine)
{
    if (selectedFields == NULL) {
        *parsedLine = parse_csv(line, delim);
        if (*parsedLine == NULL) {
            return CSV_HANDLER__OUT_OF_MEMORY;
        }

        return CSV_HANDLER__OK;
    }

    char **dumParsed = parse_csv(line, delim);

    *parsedLine = malloc(sizeof(char **) * (getSelectedFieldCount() + 1));
    if (*parsedLine == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    for (int i = 0, j = 0; (j = selectedFields[i]) != -1; i++) {
        (*parsedLine)[i] = malloc(sizeof(char *) * (strlen(dumParsed[j]) + 1));
        strcpy((*parsedLine)[i], dumParsed[j]);
    }

    (*parsedLine)[getSelectedFieldCount()] = NULL;

    free_csv_line(dumParsed);

    return CSV_HANDLER__OK;
}

/**
 * Append string with new boxed value for printing to output.
 *
 * @param   outputLine
 */
static char appendBoxedValue(char **outputLine, char *newValue)
{
    int initialLen = strlen(*outputLine);
    *outputLine = realloc(*outputLine, sizeof(char) * (initialLen + 2 + width));
    // +2 is one for '|' and one for null terminator.

    if (*outputLine == NULL) {
        return CSV_HANDLER__OUT_OF_MEMORY;
    }

    // Only want to concat part of the string, so need to do some funky stuff.

    int contentLength = strlen(newValue);
    if (contentLength > width) {
        contentLength = width;
    }
    int fillerLength = width - contentLength; // Will be zero if content is larger than width.

    for (int j = 0; j < contentLength; j++) {
        if (newValue[j] == '\n') {
            // Don't display newline.  It's confusing in this context.
            (*outputLine)[initialLen + j] = ' ';
        } else {
            (*outputLine)[initialLen + j] = newValue[j];
        }
        // Note that this starts with j = 0, so overwriting the null terminator.
    }
    for (int j = 0; j < fillerLength; j++) {
        (*outputLine)[initialLen + contentLength + j] = ' ';
    }

    (*outputLine)[initialLen + width] = '|'; // Next brace.
    (*outputLine)[initialLen + width + 1] = '\0'; // Putting back in the null terminator.

    return CSV_HANDLER__OK;
}

/**
 * Get selected field count.
 */
static int getSelectedFieldCount()
{
    return (selectedFieldCount == -1) ? countHeaders : selectedFieldCount;
}

/**
 * Set the fields for transposed output.  Helper function for initialization.
 *
 * @param   fields
 */
static char setFieldsForTransposed(char *fields)
{
    if (headers != NULL) {
        return CSV_HANDLER__OK;
    }

    char rc;

    if ((rc = csv_handler_set_headers_from_line()) != CSV_HANDLER__OK) {
        return rc;
    }

    if ((rc = csv_handler_set_selected_fields(fields)) != CSV_HANDLER__OK) {
        return rc;
    }

    return CSV_HANDLER__OK;
}

/**
 * Get field name from position number.
 *
 * @param   pos
 */
static char *getHeaderFromPosition(int pos)
{
    if (selectedFieldCount == -1) {
        return headers[pos];
    } else {
        return headers[selectedFields[pos]];
    }
}

/**
 * "Unparse" a specific value (i.e., cell), by surrounding with double-quotes if
 * necessary and doubling double-quotes if necessary.
 *
 * @param   value
 */
static char unparseValue(char **value)
{
    char dontParse = 1;
    char doubleQuotes = 0;
    for (long int i = 0; (*value)[i]; i++) {
        if ((*value)[i] == ',' || (*value)[i] == '\n') {
            dontParse = 0;
        } else if ((*value)[i] == '"') {
            dontParse = 0;
            doubleQuotes++;
        }
    }

    if (dontParse) {
        return CSV_HANDLER__OK;
    }

    char *newValue = malloc(sizeof(char) * (strlen(*value) + doubleQuotes + 3));
    // +1 for null term, +2 for opening and closing double quotes.
    strcpy(newValue, "\"");

    for (long int i = 0, j = 1; (*value)[i] != '\0'; i++) {
        newValue[j] = (*value)[i];
        if (newValue[j] == '"') {
            newValue[++j] = '"';
        }
        j++;
    }

    newValue[strlen(*value) + doubleQuotes + 1] = '\0';
    strcat(newValue, "\"");

    free(*value);
    *value = newValue;

    return CSV_HANDLER__OK;
}
