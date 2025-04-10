#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // For memcpy, memset
#include <limits.h> // For INT_MIN, INT_MAX

// Define BITSET_WORD_SIZE
#define BITSET_WORD_SIZE (sizeof(unsigned long) * 8)

// --- Struct Definition ---
typedef struct
{
    unsigned long *data; // Array to store bits
    size_t size;         // Number of bits in the bitset
    bool is_negative;    // Flag to indicate if the number is negative
} Bitset;

// --- Global Mask ---
Bitset *mask_0110 = NULL;

// --- Function Prototypes ---
char *bitset_to_string_grouped_bcd(const Bitset *bitset);
long long bcd_to_int(const Bitset *bs);
Bitset *bitset_create(size_t size);
void bitset_free(Bitset *bitset);
void bitset_set(Bitset *bitset, size_t index, bool value);
bool bitset_test(const Bitset *bitset, size_t index);
Bitset *bitset_resize(const Bitset *bitset, size_t new_size, bool keep_sign);
Bitset *bitset_copy(const Bitset *original);
Bitset *bitset_add_with_carry(const Bitset *a, const Bitset *b); // Resizing Add
void bitset_shift_left(Bitset *bitset, size_t shift);
char *bitset_to_string_normal(const Bitset *bitset);
int bitset_compare(const Bitset *a, const Bitset *b);
Bitset *bitset_subtract_magnitude(const Bitset *a, const Bitset *b, bool *result_is_negative);
Bitset *int_to_bitset(int number);
Bitset *bcd_multiply_magnitude(const Bitset *a, const Bitset *b);
Bitset *bitset_trim_leading_zeros(const Bitset *original);
bool bitset_is_zero(const Bitset *bs); // For shortcut


// --- Function Implementations ---

Bitset *bitset_create(size_t size)
{
    Bitset *bitset = (Bitset *)malloc(sizeof(Bitset));
    if (!bitset) { fprintf(stderr,"Error: malloc failed for Bitset struct\n"); return NULL; }
    bitset->size = size;
    bitset->is_negative = false;
    size_t num_words = (size + BITSET_WORD_SIZE - 1) / BITSET_WORD_SIZE;
    bitset->data = (unsigned long *)calloc(num_words, sizeof(unsigned long));
    if (!bitset->data && size > 0) {
        fprintf(stderr,"Error: calloc failed for Bitset data (size %zu)\n", size);
        free(bitset);
        return NULL;
    }
    return bitset;
}

void bitset_free(Bitset *bitset)
{
    if (bitset != NULL)
    {
        free(bitset->data);
        free(bitset);
    }
}

// Creates a new copy of a bitset
Bitset *bitset_copy(const Bitset *original) {
    if (!original) return NULL;
    Bitset *copy = bitset_create(original->size);
    if (!copy) return NULL; // Allocation failed
    copy->is_negative = original->is_negative;
    size_t num_words = (original->size + BITSET_WORD_SIZE - 1) / BITSET_WORD_SIZE;
    if (original->data && copy->data) { // Check if data pointers are valid
        memcpy(copy->data, original->data, num_words * sizeof(unsigned long));
    } else if (original->size > 0) {
        // This case shouldn't happen if create is correct, but handle defensively
        fprintf(stderr, "Warning: Copying bitset with NULL data pointer but size > 0.\n");
        // Data in copy is already zeroed by calloc in bitset_create
    }
    return copy;
}


void bitset_set(Bitset *bitset, size_t index, bool value)
{
    if (!bitset || !bitset->data || index >= bitset->size) // Added check for bitset->data
        return;
    size_t word_index = index / BITSET_WORD_SIZE;
    size_t bit_index = index % BITSET_WORD_SIZE;
    if (value)
        bitset->data[word_index] |= (1UL << bit_index);
    else
        bitset->data[word_index] &= ~(1UL << bit_index);
}


bool bitset_test(const Bitset *bitset, size_t index)
{
    if (!bitset || !bitset->data || index >= bitset->size) // Added check for bitset->data
        return false;
    size_t word_index = index / BITSET_WORD_SIZE;
    size_t bit_index = index % BITSET_WORD_SIZE;
    return (bitset->data[word_index] & (1UL << bit_index)) != 0;
}

Bitset *bitset_resize(const Bitset *bitset, size_t new_size, bool keep_sign)
{
    // Handle case where original is NULL
    if (!bitset) {
        Bitset* new_bs = bitset_create(new_size);
        // Sign defaults to false, which is fine if original was NULL (conceptually zero)
        return new_bs;
    }

    Bitset *resized = bitset_create(new_size);
    if (!resized) return NULL; // Allocation failed

    if (keep_sign) {
        resized->is_negative = bitset->is_negative;
    } // else it defaults to false

    // Copy bits efficiently using memcpy
    size_t min_size = (bitset->size < new_size) ? bitset->size : new_size;
    if (min_size > 0 && bitset->data && resized->data) {
        size_t words_to_copy = (min_size + BITSET_WORD_SIZE - 1) / BITSET_WORD_SIZE;
        memcpy(resized->data, bitset->data, words_to_copy * sizeof(unsigned long));

        // Mask any extra bits in the last copied word if sizes aren't word-aligned
        if (min_size % BITSET_WORD_SIZE != 0 && words_to_copy > 0) {
            size_t bits_in_last_word = min_size % BITSET_WORD_SIZE;
            unsigned long mask = (1UL << bits_in_last_word) - 1;
            resized->data[words_to_copy - 1] &= mask;
        }
    }
    // If new_size > bitset->size, the extra bits/words are already zero from calloc

    return resized;
}

// Original BCD addition that RESIZES on carry out
Bitset *bitset_add_with_carry(const Bitset *a, const Bitset *b)
{
    if (!a || !b) {
        fprintf(stderr, "Error: NULL parameter passed to bitset_add_with_carry.\n");
        return NULL;
    }
    if (a->size != b->size) {
        fprintf(stderr, "Error: bitset_add_with_carry requires bitsets of equal size (%zu != %zu).\n", a->size, b->size);
        return NULL;
    }
    if (a->size == 0) { // Handle empty input case
        return bitset_create(0);
    }


    Bitset *result = bitset_create(a->size); // Start with same size
    if (!result) return NULL;

    bool inter_digit_carry = false; // Carry between BCD digits

    // Iterate through each 4-bit BCD digit (from LSB to MSB)
    for (size_t i = 0; i < a->size; i += 4)
    {
        bool digit_carry_in = inter_digit_carry;
        inter_digit_carry = false; // Reset carry OUT for this digit (may be set below)
        bool bit_level_carry = digit_carry_in; // Carry for low-level binary add within digit

        // Process each of the 4 bits in the BCD digit
        for (size_t j = 0; j < 4; j++)
        {
            size_t current_bit_index = i + j;
            // Important: Stop if we go beyond the current result size
            if (current_bit_index >= result->size) break;

            bool bit_a = bitset_test(a, current_bit_index);
            bool bit_b = bitset_test(b, current_bit_index);

            bool sum_bit = bit_a ^ bit_b ^ bit_level_carry;
            bit_level_carry = (bit_a && bit_b) || (bit_a && bit_level_carry) || (bit_b && bit_level_carry);
            bitset_set(result, current_bit_index, sum_bit);
        }

        // Check if BCD correction is needed
        bool needs_correction = bit_level_carry;
        if (!needs_correction)
        {
            unsigned sum_value = 0;
            for (size_t j = 0; j < 4; j++) {
                size_t current_bit_index = i + j;
                if (current_bit_index >= result->size) break;
                if (bitset_test(result, current_bit_index)) { sum_value += (1 << j); }
            }
            needs_correction = (sum_value > 9);
        }

        // Apply BCD correction (add 0110) if needed
        if (needs_correction)
        {
            inter_digit_carry = true; // Correction generates carry to next BCD digit
            bool correction_carry = false;
            for (size_t j = 0; j < 4; j++)
            {
                size_t current_bit_index = i + j;
                if (current_bit_index >= result->size) break;

                bool correction_bit = (j == 1 || j == 2); // 0110
                bool current_bit = bitset_test(result, current_bit_index);
                bool new_bit = current_bit ^ correction_bit ^ correction_carry;
                correction_carry = (current_bit && correction_bit) || (current_bit && correction_carry) || (correction_bit && correction_carry);
                bitset_set(result, current_bit_index, new_bit);
            }
        }
    } // End loop for digits

    // Handle final carry out by RESIZING
    if (inter_digit_carry)
    {
        // printf("Warning: Overflow detected during BCD addition. Resizing.\n"); // Optional
        size_t old_size = result->size;
        Bitset *resized_result = bitset_resize(result, old_size + 4, false); // New digit is '1'
        if (!resized_result) {
            fprintf(stderr, "Error: Failed to resize result for carry.\n");
            // result still holds value without final carry bit set
            return result; // Return potentially truncated result
        }
        bitset_free(result); // Free old smaller result
        result = resized_result;
        // Set the first bit of the new digit (which represents '1')
        bitset_set(result, old_size, true); // Set bit at index old_size (LSB of new digit)
    }

    return result;
}


void bitset_shift_left(Bitset *bitset, size_t shift)
{
    if (!bitset || shift == 0) return;
    if (shift >= bitset->size) { // Shifting all bits out
        size_t num_words = (bitset->size + BITSET_WORD_SIZE - 1) / BITSET_WORD_SIZE;
        if(bitset->data) memset(bitset->data, 0, num_words * sizeof(unsigned long));
        return;
    }

    // Perform the shift (simple loop, efficient methods exist)
    for (size_t i = bitset->size - 1; ; i--) // Loop condition handled inside
    {
        bitset_set(bitset, i, (i >= shift) ? bitset_test(bitset, i - shift) : false);
        if (i == shift) break; // Prevent underflow in i - shift
    }
    // Lower bits are already set to false by the loop logic
}


char *bitset_to_string_normal(const Bitset *bitset)
{
    if (!bitset || bitset->size == 0) {
        // Return "0" for empty bitset? Or ""? Let's return "0000" to represent zero BCD.
        // Or maybe let trim handle zero representation. Return "" if truly size 0.
        char *empty_str = (char *)malloc(1);
        if (empty_str) empty_str[0] = '\0';
        return empty_str;
    }
    // Allocate size + 1 for null terminator
    char *str = (char *)malloc(bitset->size + 1);
    if (!str) return NULL;

    for (size_t i = 0; i < bitset->size; i++)
    {
        // Print bits with MSB first in string (index size-1-i)
        str[i] = bitset_test(bitset, bitset->size - 1 - i) ? '1' : '0';
    }
    str[bitset->size] = '\0'; // Null terminate
    return str;
}


// Compares MAGNITUDES only
int bitset_compare(const Bitset *a, const Bitset *b)
{
    if (a == NULL && b == NULL) return 0;
    if (a == NULL) return -1; // Consider NULL smaller
    if (b == NULL) return 1;

    // Find effective size by ignoring leading zeros
    size_t max_a_bit = a->size;
    while(max_a_bit > 0 && !bitset_test(a, max_a_bit - 1)) { max_a_bit--; }
    size_t max_b_bit = b->size;
    while(max_b_bit > 0 && !bitset_test(b, max_b_bit - 1)) { max_b_bit--; }

    if (max_a_bit > max_b_bit) return 1;
    if (max_a_bit < max_b_bit) return -1;

    // If effective sizes are the same, compare bit by bit from MSB down
    if (max_a_bit == 0) return 0; // Both are zero

    for (size_t i = max_a_bit; i-- > 0;) {
        bool bit_a = bitset_test(a, i);
        bool bit_b = bitset_test(b, i);
        if (bit_a && !bit_b) return 1;
        if (!bit_a && bit_b) return -1;
    }
    return 0; // Magnitudes are equal
}

Bitset *bitset_subtract_magnitude(const Bitset *a, const Bitset *b, bool *result_is_negative)
{

    *result_is_negative = false;
    if (!a || !b) { fprintf(stderr,"Error: NULL input.\n"); printf("--- Exiting subtract_magnitude (ERROR) ---\n"); return NULL;}

    int cmp = bitset_compare(a, b);


    const Bitset *larger = (cmp >= 0) ? a : b;
    const Bitset *smaller = (cmp >= 0) ? b : a;
    if (cmp < 0) { *result_is_negative = true; }


    size_t common_size = (larger->size > smaller->size) ? larger->size : smaller->size;
    if (common_size == 0) common_size = 4;
    if (common_size % 4 != 0) { common_size = ((common_size + 3) / 4) * 4; }


    Bitset *larger_padded = NULL, *smaller_padded = NULL;
    Bitset *nines_comp_smaller = NULL, *one = NULL, *tens_comp_smaller = NULL;
    Bitset *sum = NULL;
    Bitset *final_result = NULL;
    Bitset *operand1 = NULL, *operand2 = NULL; // Pointers for the final addition operands
    bool operand1_resized = false, operand2_resized = false; // Track if resizing happened
    size_t final_add_size = 0; // Initialize

    larger_padded = bitset_resize(larger, common_size, false);
    smaller_padded = bitset_resize(smaller, common_size, false);
    if (!larger_padded || !smaller_padded) { /* Error handling */ goto subtract_cleanup_reverted; }

    // Step 1: 9's complement
    nines_comp_smaller = bitset_create(common_size);
    if (!nines_comp_smaller) goto subtract_cleanup_reverted;
    for (size_t i = 0; i < common_size; i += 4) {
        unsigned digit_smaller = 0;
        for (size_t j = 0; j < 4; j++) { if (i + j < smaller_padded->size && bitset_test(smaller_padded, i + j)) digit_smaller |= (1 << j); }
        unsigned nines_digit = 9 - digit_smaller;
        for (size_t j = 0; j < 4; j++) { if (i+j < nines_comp_smaller->size) bitset_set(nines_comp_smaller, i + j, (nines_digit >> j) & 1); }
    }


    // Step 2: Calculate 10's complement (potentially resizing)
    one = bitset_create(common_size);
    if(!one) goto subtract_cleanup_reverted;
    bitset_set(one, 0, true);

    tens_comp_smaller = bitset_add_with_carry(nines_comp_smaller, one); // Uses resizing add
    if (!tens_comp_smaller) { /* Error handling */ goto subtract_cleanup_reverted; }


    // Determine size needed for final add based on potentially resized 10s comp
    final_add_size = (larger_padded->size > tens_comp_smaller->size) ? larger_padded->size : tens_comp_smaller->size;

    operand1 = larger_padded;
    operand2 = tens_comp_smaller;
    if (larger_padded->size < final_add_size) {

        operand1 = bitset_resize(larger_padded, final_add_size, false);
        if (!operand1) goto subtract_cleanup_reverted;
        operand1_resized = true;
    }
    if (tens_comp_smaller->size < final_add_size) {

        operand2 = bitset_resize(tens_comp_smaller, final_add_size, false);
        if (!operand2) goto subtract_cleanup_reverted;
        operand2_resized = true;
    }


    // Step 3: Add resized operands using resizing add

    sum = bitset_add_with_carry(operand1, operand2); // Uses resizing add
    if (!sum) { /* Error handling */ goto subtract_cleanup_reverted; }


    // Step 4: Check end-around carry based on size difference
    bool end_around_carry = (sum->size > final_add_size);


    // Step 5: Determine final magnitude
    if (end_around_carry) {
        // Positive result: Discard carry by resizing sum back to final_add_size
        // printf("Positive result path (discarding carry).\n"); // Debug
        final_result = bitset_resize(sum, final_add_size, false);
        if (!final_result) goto subtract_cleanup_reverted;
    } else {
        // No end-around carry
        if (cmp == 0) {
            // printf("Zero result path (inputs equal).\n"); // Debug
            final_result = bitset_create(4);
            if (!final_result) goto subtract_cleanup_reverted;
        } else {
            // Negative result path (cmp < 0)
            // Magnitude = 10s_comp(sum) calculated relative to final_add_size

            Bitset *nines_sum = bitset_create(final_add_size); // Use final_add_size
            Bitset *one_sum = bitset_create(final_add_size);   // Use final_add_size
            if (!nines_sum || !one_sum) { /* Error handling */ goto subtract_cleanup_reverted; }
            bitset_set(one_sum, 0, true);

            // Calculate 9's complement of 'sum' relative to final_add_size
            for (size_t i = 0; i < final_add_size; i += 4) {
                unsigned digit_sum = 0;
                for (size_t j = 0; j < 4; j++) { if ((i + j) < sum->size && bitset_test(sum, i + j)) digit_sum |= (1 << j); }
                unsigned nines_digit = 9 - digit_sum;
                for (size_t j = 0; j < 4; j++) { bitset_set(nines_sum, i + j, (nines_digit >> j) & 1); }
            }

            // Add 1 to get 10s complement using resizing add
            // printf("  Adding 1 to 9s_comp of sum...\n"); // Debug
            final_result = bitset_add_with_carry(nines_sum, one_sum); // Uses resizing add

            bitset_free(nines_sum);
            bitset_free(one_sum);
            if (!final_result) goto subtract_cleanup_reverted;

        }
    }

    subtract_cleanup_reverted:
    // Central cleanup point
    bitset_free(larger_padded);
    bitset_free(smaller_padded);
    bitset_free(nines_comp_smaller);
    bitset_free(one);
    bitset_free(tens_comp_smaller); // Free the potentially resized 10s comp
    bitset_free(sum);             // Free the potentially resized sum
    // Free resized operands *only if they were created*
    if (operand1_resized) bitset_free(operand1);
    if (operand2_resized) bitset_free(operand2);
    // final_result is the return value, don't free here

    // Set sign on the final result (if it exists)...
    if (final_result) {
        bool is_zero_flag = true;
        for(size_t k=0; k<final_result->size; ++k) if(bitset_test(final_result, k)) { is_zero_flag = false; break;}
        if(is_zero_flag) final_result->is_negative = false;
        else final_result->is_negative = *result_is_negative;

    } else { /* Error message */ }


    return final_result;
}
// --- End Reverted Subtract Magnitude ---

Bitset *int_to_bitset(int number)
{
    bool is_neg = false;
    unsigned int abs_number;

    if (number < 0)
    {
        is_neg = true;
        if (number == INT_MIN) { abs_number = (unsigned int)INT_MAX + 1; }
        else { abs_number = (unsigned int)(-number); }
    } else {
        abs_number = (unsigned int)number;
    }

    unsigned int temp = abs_number;
    int digit_count = (temp == 0) ? 1 : 0;
    while (temp > 0) { temp /= 10; digit_count++; }

    size_t bcd_size = digit_count * 4;
    Bitset *bitset = bitset_create(bcd_size);
    if (!bitset) return NULL;

    bitset->is_negative = is_neg;

    temp = abs_number;
    for (int i = 0; i < digit_count; i++) {
        int digit = temp % 10;
        temp /= 10;
        for (int j = 0; j < 4; j++) {
            if ((digit >> j) & 1) bitset_set(bitset, (i * 4) + j, true);
            // else: bitset_set(bitset, (i * 4) + j, false); // Already zero from calloc
        }
    }
    return bitset;
}

// Multiplication Magnitude (using repeated addition and resizing add)
// Multiplication Magnitude (using repeated addition and resizing add)
Bitset *bcd_multiply_magnitude(const Bitset *a, const Bitset *b)
{
    if (!a || !b) return NULL;

    size_t size_a = ((a->size + 3) / 4) * 4; if (size_a == 0) size_a = 4;
    size_t size_b = ((b->size + 3) / 4) * 4; if (size_b == 0) size_b = 4;
    Bitset *a_padded = NULL, *b_padded = NULL; // Initialize to NULL for cleanup
    Bitset *total_product = NULL; // Initialize to NULL
    Bitset *partial_sum_for_digit = NULL; // Initialize to NULL
    Bitset *a_add_operand = NULL; // Initialize to NULL
    Bitset *shifted_partial = NULL; // Initialize to NULL
    Bitset *temp_sum = NULL; // Initialize to NULL
    Bitset *total_op = NULL; // Initialize to NULL
    Bitset *partial_op = NULL; // Initialize to NULL
    Bitset *new_total = NULL; // Initialize to NULL
    bool total_resized = false, partial_resized = false; // Initialize

    a_padded = bitset_resize(a, size_a, false);
    b_padded = bitset_resize(b, size_b, false);
    if (!a_padded || !b_padded) { /* Error handling */ goto multiplication_cleanup; }

    size_t result_size = size_a + size_b; // Max possible size
    total_product = bitset_create(result_size);
    if (!total_product) { /* Error handling */ goto multiplication_cleanup; }

    // Loop through digits of multiplier (b)
    for (size_t i = 0; i < size_b; i += 4)
    {
        // Reset potentially allocated pointers for this loop iteration's partial products
        bitset_free(partial_sum_for_digit); partial_sum_for_digit = NULL;
        bitset_free(a_add_operand); a_add_operand = NULL;
        bitset_free(shifted_partial); shifted_partial = NULL;
        bitset_free(temp_sum); temp_sum = NULL;
        bitset_free(total_op); total_op = NULL;
        bitset_free(partial_op); partial_op = NULL;
        bitset_free(new_total); new_total = NULL;
        total_resized = false; partial_resized = false;


        unsigned digit_b = 0;
        for (size_t j = 0; j < 4; j++) { if (i + j < b_padded->size && bitset_test(b_padded, i + j)) digit_b |= (1 << j); }
        if (digit_b == 0) continue;
        if (digit_b > 9) { fprintf(stderr, "Warning: Invalid BCD digit %u in multiplier. Skipping.\n", digit_b); continue; }

        // Calculate partial product: a * digit_b (repeated addition)
        partial_sum_for_digit = bitset_create(result_size);
        a_add_operand = bitset_resize(a_padded, result_size, false); // a padded to result size
        if (!partial_sum_for_digit || !a_add_operand) { goto multiplication_cleanup; }

        for (unsigned k = 0; k < digit_b; ++k) {
            temp_sum = bitset_add_with_carry(partial_sum_for_digit, a_add_operand); // Resizing add
            if (!temp_sum) { goto multiplication_cleanup; }
            bitset_free(partial_sum_for_digit);
            partial_sum_for_digit = temp_sum; // May have grown
            temp_sum = NULL; // Prevent double free in cleanup
        }
        bitset_free(a_add_operand); a_add_operand = NULL; // Free the operand used in loop

        // Shift/Position partial product and add to total
        shifted_partial = bitset_create(result_size); // Target size
        if (!shifted_partial) { goto multiplication_cleanup; }
        for(size_t bit_idx = 0; bit_idx < partial_sum_for_digit->size; ++bit_idx) {
            if (i + bit_idx < shifted_partial->size) bitset_set(shifted_partial, i + bit_idx, bitset_test(partial_sum_for_digit, bit_idx));
            // Else: Bits shifted beyond result_size are ignored
        }
        bitset_free(partial_sum_for_digit); partial_sum_for_digit = NULL; // Free the unshifted version

        // Add to total product, resizing operands if needed
        size_t add_size = (total_product->size > shifted_partial->size) ? total_product->size : shifted_partial->size;
        total_op = total_product; // Assume no resize initially
        partial_op = shifted_partial; // Assume no resize initially
        total_resized = false;
        partial_resized = false;


        if(total_product->size < add_size) {
            total_op = bitset_resize(total_product, add_size, false);
            if (!total_op) { goto multiplication_cleanup; } // Check resize result
            total_resized = true;
        }
        if(shifted_partial->size < add_size) {
            partial_op = bitset_resize(shifted_partial, add_size, false);
            if (!partial_op) { // Check resize result
                if(total_resized) bitset_free(total_op); // Free the other resized op if needed
                goto multiplication_cleanup;
            }
            partial_resized = true;
        }

        new_total = bitset_add_with_carry(total_op, partial_op); // Resizing add

        // Cleanup for this addition step
        if(total_resized) bitset_free(total_op); total_op = NULL; // Free resized copy if created
        if(partial_resized) bitset_free(partial_op); partial_op = NULL; // Free resized copy if created
        // Don't free original total_product/shifted_partial here if resize didn't happen,
        // they are handled below or in the next loop iteration setup.

        if (!new_total) { // Check if the add failed
            // Need to free originals if resize didn't happen
            if (!total_resized) bitset_free(total_product); total_product = NULL;
            if (!partial_resized) bitset_free(shifted_partial); shifted_partial = NULL;
            goto multiplication_cleanup;
        }

        // Free the *previous* total product before assigning the new one
        bitset_free(total_product);
        total_product = new_total; // Update total product pointer
        new_total = NULL; // Prevent double free in cleanup

        // Free the original shifted_partial (if not resized and freed above)
        bitset_free(shifted_partial); shifted_partial = NULL;


    } // End loop multiplier digits

    // Success path
    goto multiplication_cleanup; // Use common cleanup

    multiplication_cleanup:
    // Define the cleanup label here
    if (a_add_operand) bitset_free(a_add_operand);
    if (partial_sum_for_digit) bitset_free(partial_sum_for_digit);
    if (shifted_partial) bitset_free(shifted_partial);
    if (temp_sum) bitset_free(temp_sum);
    if (total_op && total_resized) bitset_free(total_op); // Free if points to resized copy
    if (partial_op && partial_resized) bitset_free(partial_op); // Free if points to resized copy
    if (new_total) bitset_free(new_total); // Should be NULL if success/moved to total_product
    if (a_padded) bitset_free(a_padded);
    if (b_padded) bitset_free(b_padded);

    // Check if total_product exists (might be NULL if error occurred early)
    if (!total_product && (a && b && !bitset_is_zero(a) && !bitset_is_zero(b))) { // Check if error occurred when result shouldn't be NULL
        fprintf(stderr, "Multiplication resulted in NULL, likely due to error.\n");
    } else if (!total_product) {
        // Handle case where inputs might have been zero, resulting in NULL intentionally?
        // Better to return a zero bitset if inputs were valid but result is NULL.
        // Let's assume the caller handles NULL check or the shortcut handles zero mult.
    }


    return total_product; // Return whatever total_product points to (NULL on error)
}


/**
 * @brief Creates a new bitset by removing leading zero BCD digits.
 */
Bitset *bitset_trim_leading_zeros(const Bitset *original) {
    if (!original) return NULL;
    // Handle zero size - return "0000"
    if (original->size == 0 || original->size < 4) { // Also handle sizes too small for a digit
        Bitset* zero_bs = bitset_create(4);
        if (zero_bs) zero_bs->is_negative = false; // Zero is not negative
        return zero_bs;
    }

    size_t first_non_zero_digit_start = original->size;
    bool found_non_zero_digit = false;

    long start_check_index = ((long)original->size - 1);
    start_check_index = (start_check_index / 4) * 4;

    for (long i = start_check_index; i >= 0; i -= 4) {
        bool digit_is_zero = true;
        for (size_t j = 0; j < 4; ++j) {
            size_t current_bit_index = (size_t)i + j;
            if (current_bit_index < original->size && bitset_test(original, current_bit_index)) {
                digit_is_zero = false;
                break;
            }
        }
        if (!digit_is_zero) {
            first_non_zero_digit_start = (size_t)i;
            found_non_zero_digit = true;
            break;
        }
    }

    size_t new_size;
    if (found_non_zero_digit) {
        new_size = first_non_zero_digit_start + 4;
        if (new_size > original->size) new_size = original->size;
    } else {
        new_size = 4; // Represent as "0000"
    }

    // Optimization: If no trimming needed (and not zero), return copy
    if (new_size == original->size && found_non_zero_digit) {
        return bitset_copy(original);
    }
    // Optimization: If result is zero and original was already minimal zero, return copy
    if (!found_non_zero_digit && original->size == 4) {
        Bitset* copy = bitset_copy(original);
        if (copy) copy->is_negative = false; // Ensure zero not negative
        return copy;
    }

    // Create the new trimmed bitset
    Bitset *trimmed = bitset_create(new_size);
    if (!trimmed) return NULL;

    trimmed->is_negative = found_non_zero_digit ? original->is_negative : false;

    if (found_non_zero_digit) {
        size_t words_to_copy = (new_size + BITSET_WORD_SIZE - 1) / BITSET_WORD_SIZE;
        if (original->data && trimmed->data) {
            memcpy(trimmed->data, original->data, words_to_copy * sizeof(unsigned long));
            if (new_size % BITSET_WORD_SIZE != 0 && words_to_copy > 0) {
                size_t bits_in_last = new_size % BITSET_WORD_SIZE;
                unsigned long mask = (bits_in_last == 0) ? (~0UL) : ((1UL << bits_in_last) - 1);
                trimmed->data[words_to_copy - 1] &= mask;
            }
        }
    }
    // else: Zero case - data is already zeroed by create

    return trimmed;
}

/**
 * @brief Checks if a bitset represents the value zero.
 */
bool bitset_is_zero(const Bitset *bs) {
    if (!bs) return false; // Treat NULL as non-zero
    // Efficient check: iterate through data words
    size_t num_words = (bs->size + BITSET_WORD_SIZE - 1) / BITSET_WORD_SIZE;
    if (!bs->data && bs->size > 0) return false; // Data NULL but size > 0? Not zero.
    if (!bs->data) return true; // NULL data and size 0 is zero

    for (size_t i = 0; i < num_words; ++i) {
        unsigned long word = bs->data[i];
        // Mask off bits beyond bs->size in the last word
        if (i == num_words - 1 && bs->size % BITSET_WORD_SIZE != 0) {
            size_t bits_in_last = bs->size % BITSET_WORD_SIZE;
            unsigned long mask = (1UL << bits_in_last) - 1;
            word &= mask;
        }
        if (word != 0) {
            return false; // Found non-zero word (within size)
        }
    }
    return true; // All relevant words/bits are zero
}

// <<< NEW FUNCTION DEFINITION: Creates a BCD string with spaces >>>
char *bitset_to_string_grouped_bcd(const Bitset *bitset) {
    if (!bitset || bitset->size == 0) {
        // Return a default "0000" if input is empty or NULL
        char *zero_str = (char *)malloc(5);
        if (zero_str) strcpy(zero_str, "0000"); else return NULL;
        return zero_str;
    }
    // Ensure size is at least 4 for BCD representation, pad if necessary conceptually for loop
    size_t effective_size = (bitset->size < 4 && !bitset_is_zero(bitset)) ? 4 : bitset->size; // Make size at least 4 unless it's truly zero and already small
    if (bitset->size > 0 && bitset->size < 4) effective_size = 4; // Force padding display for small non-zeros
    if (bitset_is_zero(bitset) && bitset->size != 4) effective_size = 4; // Force "0000" if it's zero but wrong size

    // Calculate size needed for string (bits + spaces + null)
    size_t num_spaces = (effective_size > 0) ? (effective_size - 1) / 4 : 0;
    size_t str_len = effective_size + num_spaces;
    char *str = (char *)malloc(str_len + 1);
    if (!str) return NULL;

    size_t str_idx = 0;
    // Iterate from MSB down to LSB (conceptually padding with '0' if effective_size > bitset->size)
    for (long i = (long)effective_size - 1; i >= 0; --i) {
        // Get bit value, defaulting to false (0) if beyond original bitset size
        bool bit_val = ((size_t)i < bitset->size) ? bitset_test(bitset, i) : false;
        str[str_idx++] = bit_val ? '1' : '0';

        // Add space if we are not at the end AND the next bit starts a new group (index i is divisible by 4)
        if (i > 0 && (i % 4 == 0)) {
            str[str_idx++] = ' ';
        }
    }
    str[str_idx] = '\0'; // Null terminate

    return str;
}


// <<< NEW FUNCTION DEFINITION: Converts BCD magnitude to integer >>>
long long bcd_to_int(const Bitset *bs) {
    if (!bs) return -1; // Error: NULL input

    long long result = 0;
    long long power_of_10 = 1;

    // Iterate through the bitset in 4-bit chunks (digits) from LSB to MSB
    for (size_t i = 0; i < bs->size; i += 4) {
        unsigned digit_value = 0;
        // Extract the 4 bits for the current digit
        for (size_t j = 0; j < 4; ++j) {
            size_t current_bit_index = i + j;
            if (current_bit_index < bs->size) { // Check bounds
                if (bitset_test(bs, current_bit_index)) {
                    digit_value |= (1 << j); // Build binary value of the 4 bits
                }
            }
        }

        // Check if the digit is valid BCD (0-9)
        if (digit_value > 9) {
            fprintf(stderr, "Warning: Invalid BCD digit detected (%u) during conversion to int.\n", digit_value);
            // Return error
            return -1;
        }

        // Add the digit's value times the appropriate power of 10
        // Check for potential overflow *before* calculation
        long long term;
        if (__builtin_mul_overflow((long long)digit_value, power_of_10, &term)) { // Check multiplication overflow
            fprintf(stderr, "Warning: Overflow calculating term in bcd_to_int.\n");
            return -1;
        }
        if (__builtin_add_overflow(result, term, &result)) { // Check addition overflow
            fprintf(stderr, "Warning: Overflow adding term in bcd_to_int.\n");
            // Revert result? No easy way, just return error
            return -1;
        }

        // Check power_of_10 for overflow *before* next multiplication
        if (i + 4 < bs->size) { // Only check if there's a next digit
            if (power_of_10 > LLONG_MAX / 10) {
                fprintf(stderr, "Warning: Potential overflow multiplying power_of_10 in bcd_to_int.\n");
                return -1;
            }
            power_of_10 *= 10;
        }
    }
    return result;
}

// --- Main Function (With Zero Shortcuts) ---

int main()
{
    Bitset *num1 = NULL;
    Bitset *num2 = NULL;
    int choice;
    int input_num;

    // Initialize BCD mask (0110)
    mask_0110 = bitset_create(4);
    if (mask_0110) {
        bitset_set(mask_0110, 1, true);
        bitset_set(mask_0110, 2, true);
    } else {
        fprintf(stderr, "Failed to create BCD mask. Exiting.\n");
        return 1;
    }

    while (1)
    {
        printf("\nCurrent Numbers:\n");
        char *s1 = num1 ? bitset_to_string_normal(num1) : NULL;
        printf("Number 1: %s%s\n",
               (num1 && num1->is_negative) ? "1111 " : "",
               s1 ? s1 : "Not set");
        free(s1);

        char *s2 = num2 ? bitset_to_string_normal(num2) : NULL;
        printf("Number 2: %s%s\n",
               (num2 && num2->is_negative) ? "1111 " : "",
               s2 ? s2 : "Not set");
        free(s2);

        printf("\nMenu:\n");
        printf("1. Enter Number 1\n");
        printf("2. Enter Number 2\n");
        printf("3. Add (Number 1 + Number 2)\n");
        printf("4. Subtract (Number 1 - Number 2)\n");
        printf("5. Multiply (Number 1 * Number 2)\n");
        printf("6. Compare (Number 1 vs Number 2)\n"); // Compare including sign
        printf("7. Exit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n'); // Clear input buffer
            continue;
        }
        while (getchar() != '\n'); // Clear potential newline


        switch (choice)
        {
            case 1: // Enter Number 1
                printf("Enter integer for Number 1: ");
                if (scanf("%d", &input_num) != 1) { /* handle error */ while (getchar() != '\n'); continue; }
                while (getchar() != '\n');
                if (num1) bitset_free(num1);
                num1 = int_to_bitset(input_num);
                if (!num1) printf("Error creating bitset for Number 1.\n");
                break;

            case 2: // Enter Number 2
                printf("Enter integer for Number 2: ");
                if (scanf("%d", &input_num) != 1) { /* handle error */ while (getchar() != '\n'); continue; }
                while (getchar() != '\n');
                if (num2) bitset_free(num2);
                num2 = int_to_bitset(input_num);
                if (!num2) printf("Error creating bitset for Number 2.\n");
                break;

            case 3: // Add (Handles Signs, Zero Shortcut & Trimming)
                if (num1 && num2)
                {
                    Bitset *sum = NULL;
                    bool shortcut_taken = false;
                    bool final_sum_negative = false;
                    const char* op_name = "Sum"; // Define op name for printing

                    // --- Zero Shortcuts ---
                    if (bitset_is_zero(num2)) { sum = bitset_copy(num1); if(sum)final_sum_negative=sum->is_negative; shortcut_taken=true; }
                    else if (bitset_is_zero(num1)) { sum = bitset_copy(num2); if(sum)final_sum_negative=sum->is_negative; shortcut_taken=true; }
                    // --- End Zero Shortcuts ---

                    if (!shortcut_taken) {
                        bool calc_result_negative = false;
                        size_t max_size = (num1->size > num2->size) ? num1->size : num2->size;
                        if (max_size % 4 != 0) max_size = ((max_size + 3) / 4) * 4; if (max_size == 0) max_size = 4;
                        Bitset* n1_op = bitset_resize(num1, max_size, true); Bitset* n2_op = bitset_resize(num2, max_size, true);
                        if (!n1_op || !n2_op) { /* Error */ bitset_free(n1_op); bitset_free(n2_op); }
                        else {
                            if (n1_op->is_negative == n2_op->is_negative) { sum = bitset_add_with_carry(n1_op, n2_op); calc_result_negative = n1_op->is_negative; }
                            else { if (n1_op->is_negative) { sum = bitset_subtract_magnitude(n2_op, n1_op, &calc_result_negative); } else { sum = bitset_subtract_magnitude(n1_op, n2_op, &calc_result_negative); } }
                            bitset_free(n1_op); bitset_free(n2_op); if (sum) final_sum_negative = calc_result_negative;
                        }
                    }

                    // --- Process & Print Result ---
                    if (sum) {
                        Bitset *trimmed_sum = bitset_trim_leading_zeros(sum);
                        if (!trimmed_sum) { /* Error */ bitset_free(sum); sum = NULL; }
                        else if (trimmed_sum != sum) { bitset_free(sum); sum = trimmed_sum; }

                        if (sum) {
                            bool is_zero = bitset_is_zero(sum);
                            if (!is_zero) sum->is_negative = final_sum_negative; else sum->is_negative = false;

                            // <<< Print Block with Decimal >>>
                            char* s_bcd = bitset_to_string_grouped_bcd(sum); // Use grouped BCD string
                            long long decimal_val = bcd_to_int(sum); // Convert magnitude to decimal

                            printf("%s:\n", op_name); // Print operation name
                            printf("  BCD:     %s%s\n",
                                   sum->is_negative ? "1111 " : "", // Use 1111 for negative BCD
                                   s_bcd ? s_bcd : "Error");
                            if (decimal_val != -1) { // Check for conversion error from bcd_to_int
                                printf("  Decimal: %s%lld\n", // Use standard '-' for decimal sign
                                       sum->is_negative ? "-" : "",
                                       decimal_val);
                            } else {
                                printf("  Decimal: Error converting BCD\n");
                            }
                            free(s_bcd);
                            // <<< End Print Block >>>

                            bitset_free(sum); // Free final result
                        }
                    } else { printf("Error during calculation or shortcut.\n"); }
                } else { printf("Error: Both numbers must be set first.\n"); }
                break; // End Case 3


            case 4: // Subtract (Handles Signs, Zero Shortcut & Trimming)
                if (num1 && num2)
                {
                    Bitset *diff = NULL;
                    bool shortcut_taken = false;
                    bool final_diff_negative = false;
                    const char* op_name = "Difference"; // Define op name for printing

                    // --- Zero Shortcuts ---
                    if (bitset_is_zero(num2)) { diff = bitset_copy(num1); if(diff)final_diff_negative=diff->is_negative; shortcut_taken=true; }
                    else if (bitset_is_zero(num1)) { diff = bitset_copy(num2); if(diff)final_diff_negative=!diff->is_negative; shortcut_taken=true; }
                    // --- End Zero Shortcuts ---

                    if (!shortcut_taken) {
                        bool calc_result_negative = false;
                        size_t max_size = (num1->size > num2->size) ? num1->size : num2->size;
                        if (max_size % 4 != 0) max_size = ((max_size + 3) / 4) * 4; if (max_size == 0) max_size = 4;
                        Bitset* n1_op = bitset_resize(num1, max_size, true); Bitset* n2_flipped = bitset_resize(num2, max_size, true);
                        if (!n1_op || !n2_flipped) { /* Error */ bitset_free(n1_op); bitset_free(n2_flipped); }
                        else {
                            n2_flipped->is_negative = !n2_flipped->is_negative; // Flip sign for A+(-B) logic
                            if (n1_op->is_negative == n2_flipped->is_negative) { diff = bitset_add_with_carry(n1_op, n2_flipped); calc_result_negative = n1_op->is_negative; }
                            else { if (n1_op->is_negative) { diff = bitset_subtract_magnitude(n2_flipped, n1_op, &calc_result_negative); } else { diff = bitset_subtract_magnitude(n1_op, n2_flipped, &calc_result_negative); } }
                            bitset_free(n1_op); bitset_free(n2_flipped); if (diff) final_diff_negative = calc_result_negative;
                        }
                    }

                    // --- Process & Print Result ---
                    if (diff) {
                        Bitset *trimmed_diff = bitset_trim_leading_zeros(diff);
                        if (!trimmed_diff) { /* Error */ bitset_free(diff); diff = NULL; }
                        else if (trimmed_diff != diff) { bitset_free(diff); diff = trimmed_diff; }

                        if (diff) {
                            bool is_zero = bitset_is_zero(diff);
                            if (!is_zero) diff->is_negative = final_diff_negative; else diff->is_negative = false;

                            // <<< Print Block with Decimal >>>
                            char* s_bcd = bitset_to_string_grouped_bcd(diff); // Use grouped BCD string
                            long long decimal_val = bcd_to_int(diff); // Convert magnitude to decimal

                            printf("%s:\n", op_name); // Print operation name
                            printf("  BCD:     %s%s\n",
                                   diff->is_negative ? "1111 " : "", // Use 1111 for negative BCD
                                   s_bcd ? s_bcd : "Error");
                            if (decimal_val != -1) { // Check for conversion error from bcd_to_int
                                printf("  Decimal: %s%lld\n", // Use standard '-' for decimal sign
                                       diff->is_negative ? "-" : "",
                                       decimal_val);
                            } else {
                                printf("  Decimal: Error converting BCD\n");
                            }
                            free(s_bcd);
                            // <<< End Print Block >>>

                            bitset_free(diff); // Free final result
                        }
                    } else { printf("Error during calculation or shortcut.\n"); }
                } else { printf("Error: Both numbers must be set first.\n"); }
                break; // End Case 4


            case 5: // Multiply (Handles Signs, Zero Shortcut & Trimming)
                if (num1 && num2)
                {
                    Bitset *prod_mag = NULL;
                    bool shortcut_taken = false;
                    bool final_prod_negative = false;
                    const char* op_name = "Product"; // Define op name for printing

                    // --- Zero Shortcut ---
                    if (bitset_is_zero(num1) || bitset_is_zero(num2)) { prod_mag = bitset_create(4); final_prod_negative=false; shortcut_taken=true; }
                    // --- End Zero Shortcut ---

                    if (!shortcut_taken) {
                        prod_mag = bcd_multiply_magnitude(num1, num2);
                        if(prod_mag) final_prod_negative = (num1->is_negative != num2->is_negative);
                    }

                    // --- Process & Print Result ---
                    if (prod_mag) {
                        Bitset *trimmed_prod = bitset_trim_leading_zeros(prod_mag);
                        if (!trimmed_prod) { /* Error */ bitset_free(prod_mag); prod_mag = NULL; }
                        else if (trimmed_prod != prod_mag) { bitset_free(prod_mag); prod_mag = trimmed_prod; }

                        if (prod_mag) {
                            bool is_zero = bitset_is_zero(prod_mag);
                            if (!is_zero) prod_mag->is_negative = final_prod_negative; else prod_mag->is_negative = false;

                            // <<< Print Block with Decimal >>>
                            char* s_bcd = bitset_to_string_grouped_bcd(prod_mag); // Use grouped BCD string
                            long long decimal_val = bcd_to_int(prod_mag); // Convert magnitude to decimal

                            printf("%s:\n", op_name); // Print operation name
                            printf("  BCD:     %s%s\n",
                                   prod_mag->is_negative ? "1111 " : "", // Use 1111 for negative BCD
                                   s_bcd ? s_bcd : "Error");
                            if (decimal_val != -1) { // Check for conversion error from bcd_to_int
                                printf("  Decimal: %s%lld\n", // Use standard '-' for decimal sign
                                       prod_mag->is_negative ? "-" : "",
                                       decimal_val);
                            } else {
                                printf("  Decimal: Error converting BCD\n");
                            }
                            free(s_bcd);
                            // <<< End Print Block >>>

                            bitset_free(prod_mag); // Free final result
                        }
                    } else { printf("Error during calculation or shortcut.\n"); }
                } else { printf("Error: Both numbers must be set first.\n"); }
                break; // End Case 5

            case 6: // Compare (Handles Signs)
                if (num1 && num2)
                {
                    int final_cmp;
                    if (!num1->is_negative && num2->is_negative) final_cmp = 1;
                    else if (num1->is_negative && !num2->is_negative) final_cmp = -1;
                    else {
                        int mag_cmp = bitset_compare(num1, num2);
                        final_cmp = num1->is_negative ? -mag_cmp : mag_cmp;
                    }

                    printf("Comparison Result (Number 1 vs Number 2):\n");
                    if (final_cmp < 0) printf("Number 1 < Number 2\n");
                    else if (final_cmp > 0) printf("Number 1 > Number 2\n");
                    else printf("Number 1 == Number 2\n");
                } else { /* Error message */ }
                break;

            case 7: // Exit
                printf("Exiting.\n");
                if (num1) bitset_free(num1);
                if (num2) bitset_free(num2);
                bitset_free(mask_0110);
                return 0;

            default:
                printf("Invalid choice. Please try again.\n");
        }
    } // End while loop

    // Should not be reached
    if (num1) bitset_free(num1);
    if (num2) bitset_free(num2);
    bitset_free(mask_0110);
    return 0;
} // End main