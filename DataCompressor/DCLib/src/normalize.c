/* Value (de-)normalizer
   Part of DataCompressor
   Andreas Unterweger, 2015 */

#include "err_codes.h"
#include "io_macros.h"
#include "normalize.h"
#include "enc_dec.h"

#define ENCODER_MODE_INT_TO_INT 1
#define ENCODER_MODE_FLOAT_TO_INT 2 /* default */

#define DECODER_MODE_INT_TO_INT 3
#define DECODER_MODE_INT_TO_FLOAT 4 /* default */

io_int_t Normalize(bit_file_buffer_t * const in_bit_buf, bit_file_buffer_t * const out_bit_buf, const options_t * const options)
{

  size_t encoder_mode = ENCODER_MODE_FLOAT_TO_INT;

  if ((options->input_datatype == DATATYPE_OPTION_INT && options->output_datatype == DATATYPE_OPTION_INT)
        || (options->input_datatype == DATATYPE_OPTION_INT && options->output_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT))
  {
    encoder_mode = ENCODER_MODE_INT_TO_INT;
  }
  else if ((options->input_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT && options->output_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT )
            || (options->input_datatype == DATATYPE_OPTION_FLOAT && options->output_datatype == DATATYPE_OPTION_INT)
            || (options->input_datatype == DATATYPE_OPTION_FLOAT && options->output_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT))
  {
    encoder_mode = ENCODER_MODE_FLOAT_TO_INT;
  }
  else
  {
    return ERROR_DATATYPE;
  }


    while (!EndOfBitFileBuffer(in_bit_buf))
    {
        /*
        We have uint, int and float as input and output datatypes, wich would make 9 different cases to handle.
        Since some conversions are not needed or even dangerous, only 2 cases are implemented:
        1) int to int
        2) float to int (the same as orignal behaviour)

        Note that (1) ignores the normalization_factor but uses ignore_bits.
        On the other hand, (2) uses normalization_factor but does not use ignore_bits.

        Since uint is not supported, we can be generous with the input / output type options provided by the user::
        Case (1): encoder outputs int anyway, so it is ok if
        - both types or
        - only input as int is provided

        Case (2): encoder expects float by default, so it should work when
        - nothing is set (compatibility) or
        - both types are set correctly or
        - only input is set to float.

        anything else will cause an error.
        */

        if (encoder_mode == ENCODER_MODE_INT_TO_INT)
        {
            io_int_t value, normalized_value;
            const size_t value_size = 8 * sizeof(value);

            READ_BITS_CHECKED((uint8_t * const)&value, value_size, in_bit_buf, options->error_log_file);

            normalized_value = value / (1u << options->ignore_bits); /* we ignore n of the least significant bits to lower the entropy */

            /* TODO: error handling */
            WRITE_SIGNED_VALUE_BITS_CHECKED((const io_int_t * const)&normalized_value, options->value_size_bits, out_bit_buf, options->error_log_file);
        }
        else if (encoder_mode == ENCODER_MODE_FLOAT_TO_INT)
        {
            /* nearly untouched original implementation, default behaviour for compatibility */
            float value;
            const size_t value_size = 8 * sizeof(value);
            io_int_t normalized_value;
            READ_BITS_CHECKED((uint8_t * const)&value, value_size, in_bit_buf, options->error_log_file);
            if (value > 0)
              value = (value * options->normalization_factor + (float)0.5); /* Round towards +inf */
            else if (value < 0)
              value = (value * options->normalization_factor - (float)0.5); /* Round towards -inf */
            if (value < (-(float)((io_uint_t)1 << (options->value_size_bits - 1))) || value > (((io_uint_t)1 << (options->value_size_bits - 1)) - 1)) /* Value range check */
              return ERROR_INVALID_VALUE;
            normalized_value = (io_int_t)value;
            WRITE_VALUE_BITS_CHECKED((const io_uint_t * const)&normalized_value, options->value_size_bits, out_bit_buf, options->error_log_file);
        }
        else
        {
            return ERROR_DATATYPE;
        }

    }
    return NO_ERROR;
}

io_int_t Denormalize(bit_file_buffer_t * const in_bit_buf, bit_file_buffer_t * const out_bit_buf, const options_t * const options)
{
  size_t decoder_mode = DECODER_MODE_INT_TO_FLOAT;

  if ((options->output_datatype == DATATYPE_OPTION_INT && options->input_datatype == DATATYPE_OPTION_INT)
        || (options->output_datatype == DATATYPE_OPTION_INT && options->input_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT))
  {
    decoder_mode = DECODER_MODE_INT_TO_INT;
  }
  else if ((options->output_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT && options->input_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT )
            || (options->output_datatype == DATATYPE_OPTION_FLOAT && options->input_datatype == DATATYPE_OPTION_INT)
            || (options->output_datatype == DATATYPE_OPTION_FLOAT && options->input_datatype == DATATYPE_OPTION_COMPATIBILITY_DEFAULT))
  {
    decoder_mode = DECODER_MODE_INT_TO_FLOAT;
  }
  else
  {
      return ERROR_DATATYPE;
  }



  while (!EndOfBitFileBuffer(in_bit_buf))
  {
        /*
        We have uint, int and float as input and output datatypes, wich would make 9 different cases to handle.
        Since some conversions are not needed or even dangerous, only 2 cases are implemented:
        3) int to int
        4) int to float (the same as orignal behaviour)

        Note that (3) ignores the normalization_factor but uses ignore_bits.
        On the other hand, (4) uses normalization_factor but does not use ignore_bits.

        Since uint is not supported, we can be generous with the input / output type options provided by the user:
        Case (3): decoder expects int anyway, so it is ok if
        - both types or
        - only input as sint is provided

        Case (4): decoder outputs float by default, so it should work when
        - nothing is set (compatibility) or
        - both types are set correctly or
        - only output is set to float.

        anything else will cause an error.
        */

        if (decoder_mode == DECODER_MODE_INT_TO_INT)
        {
            io_int_t normalized_value, denormalized_value;
            READ_SIGNED_VALUE_BITS_CHECKED((io_int_t * const)&normalized_value, options->value_size_bits, in_bit_buf, options->error_log_file);

            denormalized_value = normalized_value * (1 << options->ignore_bits); /* recover the ignored bits */

            WRITE_SIGNED_VALUE_BITS_CHECKED((const io_int_t * const)&denormalized_value, options->value_size_bits, out_bit_buf, options->error_log_file);
        }
        else if (decoder_mode == DECODER_MODE_INT_TO_FLOAT)
        {
            /* untouched original implementation, default behaviour for compatibility */
            io_int_t normalized_value;
            float denormalized_value;
            const size_t denormalized_value_size = 8 * sizeof(denormalized_value);
            READ_VALUE_BITS_CHECKED((io_uint_t * const)&normalized_value, options->value_size_bits, in_bit_buf, options->error_log_file);
            normalized_value = EXTEND_IO_INT_SIGN(normalized_value, options->value_size_bits);
            denormalized_value = (float)normalized_value / options->normalization_factor;
            WRITE_BITS_CHECKED((const uint8_t * const)&denormalized_value, denormalized_value_size, out_bit_buf, options->error_log_file);
        }
        else
        {
            return ERROR_DATATYPE;
        }

  }
  return NO_ERROR;
}
