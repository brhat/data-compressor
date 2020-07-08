/* Comma-separated value reader/writer
   Part of DataCompressor
   Andreas Unterweger, 2015 */

#include "err_codes.h"
#include "io_macros.h"
#include "csv.h"
#include "enc_dec.h"

#include <float.h>

#define FLOAT_TEXT_BUFFER_SIZE (1 /*'-'*/ + (FLT_MAX_10_EXP + 1) /*38+1 digits*/ + 1 /*'.'*/ + FLT_DIG /*Default precision*/ + 1 /*\0*/) /* Adopted from http://stackoverflow.com/questions/7235456/what-are-the-maximum-numbers-of-characters-output-by-sprintf-when-outputting-flo */

#define MAX_DECIMAL_SIZE(x)  ((size_t)(CHAR_BIT * sizeof(x) * 302 / 1000) + 1) /* Adopted from https://stackoverflow.com/questions/44023228/is-there-a-better-way-to-size-a-buffer-for-printing-integers */

io_int_t ReadCSV(bit_file_buffer_t * const in_bit_buf, bit_file_buffer_t * const out_bit_buf, const options_t * const options)
{
  size_t column = 1;
  char current_char;
  const size_t current_char_size = 8 * sizeof(current_char);
  char buffer[FLOAT_TEXT_BUFFER_SIZE] = { 0 };
  size_t used_buffer = 0;
  while (!EndOfBitFileBuffer(in_bit_buf))
  {
    READ_BITS_CHECKED((uint8_t * const)&current_char, current_char_size, in_bit_buf, options->error_log_file);
    if (current_char == options->separator_char || current_char == '\n' || EndOfBitFileBuffer(in_bit_buf))
    {
      if (EndOfBitFileBuffer(in_bit_buf) && column == options->column /* Text character from desired column */)
        buffer[used_buffer++] = current_char;
      if (column == options->column)
      {
        buffer[used_buffer++] = '\0'; /* Make sure that the string is terminated */
        switch (options->output_datatype)
        {
            case DATATYPE_OPTION_UINT:
            {
                io_uint_t value;
                const size_t value_size = 8 * sizeof(value);

                value = (io_uint_t)IO_STRTOUL(buffer, NULL, 10); /* Interpret value as unsigned integer */
                WRITE_BITS_CHECKED((const uint8_t * const)&value, value_size, out_bit_buf, options->error_log_file);
                break;
            }
            case DATATYPE_OPTION_INT:
            {
                io_int_t value;
                const size_t value_size = 8 * sizeof(value);

                value = (io_int_t)IO_STRTOLL(buffer, NULL, 10); /* Interpret value as signed integer */
                WRITE_BITS_CHECKED((const uint8_t * const)&value, value_size, out_bit_buf, options->error_log_file);
                break;
            }
            case DATATYPE_OPTION_COMPATIBILITY_DEFAULT: /* this is the same as DATATYPE_OPTION_FLOAT*/
            case DATATYPE_OPTION_FLOAT:
            {
                float value;
                const size_t value_size = 8 * sizeof(value);

                value = strtof(buffer, NULL); /* Interpret value as float */
                WRITE_BITS_CHECKED((const uint8_t * const)&value, value_size, out_bit_buf, options->error_log_file);
                break;
            }
            default:
                return ERROR_DATATYPE;
        }
        used_buffer = 0; /* Reset buffer */
      }
      column++;
    }
    else if (column == options->column) /* Text character from desired column */
      buffer[used_buffer++] = current_char;
    if (current_char == '\n') /* Next line */
      column = 1; /* Reset column */
  }
  return NO_ERROR;
}

io_int_t WriteCSV(bit_file_buffer_t * const in_bit_buf, bit_file_buffer_t * const out_bit_buf, const options_t * const options)
{
  while (!EndOfBitFileBuffer(in_bit_buf))
  {
    io_int_t retval;
    size_t i;

    for (i = 1; i < options->column; i++) /* Create empty columns if necessary */
    {
      WRITE_BITS_CHECKED((const uint8_t * const)&options->separator_char, 8 * sizeof(options->separator_char), out_bit_buf, options->error_log_file);
    }

    switch (options->input_datatype)
    {
        case DATATYPE_OPTION_UINT:
        {
            io_uint_t value;
            const size_t value_size = options->value_size_bits;
            char buffer[MAX_DECIMAL_SIZE(io_uint_t)];

            READ_VALUE_BITS_CHECKED((io_uint_t * const)&value, value_size, in_bit_buf, options->error_log_file);
            if ((retval = (io_int_t)sprintf(buffer, "%lu\n", value)) < 0)
                return ERROR_MEMORY;
            WRITE_BITS_CHECKED((const uint8_t * const)buffer, 8 * (size_t)retval, out_bit_buf, options->error_log_file); /* Write number of characters in bytes! */
            break;
        }
        case DATATYPE_OPTION_INT:
        {
            io_int_t value;
            const size_t value_size = options->value_size_bits;
            char buffer[MAX_DECIMAL_SIZE(io_int_t) + 1];

            READ_SIGNED_VALUE_BITS_CHECKED((io_int_t * const)&value, value_size, in_bit_buf, options->error_log_file);

            if ((retval = (io_int_t)sprintf(buffer, "%ld\n", value)) < 0)
                return ERROR_MEMORY;
            WRITE_BITS_CHECKED((const uint8_t * const)buffer, 8 * (size_t)retval, out_bit_buf, options->error_log_file); /* Write number of characters in bytes! */
            break;
        }
        case DATATYPE_OPTION_COMPATIBILITY_DEFAULT: /* this is the same as DATATYPE_OPTION_FLOAT*/
        case DATATYPE_OPTION_FLOAT:
        {
            float value;
            char buffer[FLOAT_TEXT_BUFFER_SIZE];
            const size_t value_size = 8 * sizeof(value);
            READ_BITS_CHECKED((uint8_t * const)&value, value_size, in_bit_buf, options->error_log_file);

            if ((retval = (io_int_t)sprintf(buffer, "%.*f\n", (int)options->num_decimal_places, value)) < 0)
                return ERROR_MEMORY;
            WRITE_BITS_CHECKED((const uint8_t * const)buffer, 8 * (size_t)retval, out_bit_buf, options->error_log_file); /* Write number of characters in bytes! */

            break;
        }
        default:
            return ERROR_DATATYPE;
    }
  }
  return NO_ERROR;
}
