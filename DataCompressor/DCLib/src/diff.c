/* Differential coder
   Part of DataCompressor
   Andreas Unterweger, 2015 */

#include "err_codes.h"
#include "io_macros.h"
#include "diff.h"
#include "enc_dec.h"

#define NORMALIZE_MODE_UNSIGNED 1
#define NORMALIZE_MODE_SIGNED 2

io_int_t EncodeDifferential(bit_file_buffer_t * const in_bit_buf, bit_file_buffer_t * const out_bit_buf, const options_t * const options)
{
  uint8_t mode = NORMALIZE_MODE_UNSIGNED;
  uint8_t recent_phase;
  io_int_t value, diff_value;
  io_int_t* last_value = (io_int_t *)malloc(options->num_phases * sizeof(io_int_t));
  if (last_value == NULL)
    return ERROR_MALLOC;

  /* this is really generous: we ignore the output_datatype option for the encoder*/
  if ((options->input_datatype == DATATYPE_OPTION_UINT) || (options->input_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT))
    mode = NORMALIZE_MODE_UNSIGNED;
  else if (options->input_datatype == DATATYPE_OPTION_INT)
    mode = NORMALIZE_MODE_SIGNED;
  else
    return ERROR_DATATYPE;

  /* init to zero */
  for (recent_phase = 0; recent_phase < options->num_phases; recent_phase++)
  {
    last_value[recent_phase] = 0;
  }
  recent_phase = 0;
  /* main loop */
  while (!EndOfBitFileBuffer(in_bit_buf))
  {
    /* read data, method depending on sign */
    if (mode == NORMALIZE_MODE_UNSIGNED)
    {
      READ_VALUE_BITS_CHECKED((io_uint_t * const)&value, options->value_size_bits, in_bit_buf, options->error_log_file);
    }
    else if (mode == NORMALIZE_MODE_SIGNED)
    {
      READ_SIGNED_VALUE_BITS_CHECKED(&value, options->value_size_bits, in_bit_buf, options->error_log_file);
    }
    else
    {
      free(last_value);
      return ERROR_DATATYPE;
    }

    /* actual encoding */
    diff_value = value - last_value[recent_phase];

    /* write data, method depending on sign */
    if (mode == NORMALIZE_MODE_UNSIGNED)
    {
      if (options->value_size_bits != IO_SIZE_BITS && (diff_value < (-(io_int_t)((io_uint_t)1 << (options->value_size_bits - 1))) || diff_value > ((((io_int_t)1 << (options->value_size_bits - 1)) - 1)))) /* Value range check */
        return ERROR_INVALID_VALUE;
      WRITE_VALUE_BITS_CHECKED((io_uint_t * const)&diff_value, options->value_size_bits, out_bit_buf, options->error_log_file);
    }
    else if (mode == NORMALIZE_MODE_SIGNED)
    {
      WRITE_SIGNED_VALUE_BITS_CHECKED(&diff_value, options->value_size_bits, out_bit_buf, options->error_log_file);
    }
    else
    {
      free(last_value);
      return ERROR_DATATYPE;
    }
    last_value[recent_phase] = value;
    recent_phase = (recent_phase < options->num_phases - 1) ? recent_phase + 1 : 0;
  }

  free(last_value);

  if (recent_phase != 0) /* if there was enough data for all phases, the next expected phase should be 0 */
    return ERROR_PHASE;

  return NO_ERROR;
}

io_int_t DecodeDifferential(bit_file_buffer_t * const in_bit_buf, bit_file_buffer_t * const out_bit_buf, const options_t * const options)
{
  uint8_t mode = NORMALIZE_MODE_UNSIGNED;
  uint8_t recent_phase;
  io_int_t value;
  io_int_t* last_value = (io_int_t *)malloc(options->num_phases * sizeof(io_int_t));
  if (last_value == NULL)
    return ERROR_MALLOC;

  if ((options->output_datatype == DATATYPE_OPTION_UINT) || (options->output_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT))
    mode = NORMALIZE_MODE_UNSIGNED;
  else if (options->output_datatype == DATATYPE_OPTION_INT)
    mode = NORMALIZE_MODE_SIGNED;
  else
    return ERROR_DATATYPE;

  /* init to zero */
  for (recent_phase = 0; recent_phase < options->num_phases; recent_phase++)
  {
    last_value[recent_phase] = 0;
  }
  recent_phase = 0;
  /* main loop */
  while (!EndOfBitFileBuffer(in_bit_buf))
  {
    /* read data, method depending on sign */
    if (mode == NORMALIZE_MODE_UNSIGNED)
    {
      READ_VALUE_BITS_CHECKED((io_uint_t * const)&value, options->value_size_bits, in_bit_buf, options->error_log_file);
      value = EXTEND_IO_INT_SIGN(value, options->value_size_bits);
    }
    else if (mode == NORMALIZE_MODE_SIGNED)
    {
      READ_SIGNED_VALUE_BITS_CHECKED(&value, options->value_size_bits, in_bit_buf, options->error_log_file);
    }
    else
    {
      free(last_value);
      return ERROR_DATATYPE;
    }

    /* actual decoding */
    value += last_value[recent_phase];

    /* write data, method depending on sign */
    if (mode == NORMALIZE_MODE_UNSIGNED)
    {
      WRITE_VALUE_BITS_CHECKED((const io_uint_t * const)&value, options->value_size_bits, out_bit_buf, options->error_log_file);
    }
    else if (mode == NORMALIZE_MODE_SIGNED)
    {
      WRITE_SIGNED_VALUE_BITS_CHECKED(&value, options->value_size_bits, out_bit_buf, options->error_log_file);
    }
    else
    {
      free(last_value);
      return ERROR_DATATYPE;
    }
    last_value[recent_phase] = value;
    recent_phase = (recent_phase < options->num_phases - 1) ? recent_phase + 1 : 0;
  }

  free(last_value);

  if (recent_phase != 0) /* if there was enough data for all phases, the next expected phase should be 0 */
    return ERROR_PHASE;
  return NO_ERROR;
}
