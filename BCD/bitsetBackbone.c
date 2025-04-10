#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define BITSET_WORD_SIZE (sizeof(unsigned long) * CHAR_BIT)

bool negative = false;
bool negative_number = false;

typedef struct
{
    unsigned long *data; // Array to store bits
    size_t size;         // Number of bits in the bitset
} Bitset;

Bitset *mask_0110 = NULL;

// Initialize a bitset with a given size
Bitset *bitset_create(size_t size);
// Free the bitset
void bitset_free(Bitset *bitset);
// Set a bit at a specific index
void bitset_set(Bitset *bitset, size_t index, bool value);
// Get the value of a bit at a specific index
bool bitset_test(const Bitset *bitset, size_t index);

// Resizing bitset
Bitset *bitset_resize(const Bitset *bitset, size_t new_size);
// Add two bitsets
Bitset *bitset_add_without_carry(const Bitset *a, const Bitset *b);
Bitset *bitset_add_with_carry(const Bitset *a, const Bitset *b);


// Shift operations
void bitset_shift_left(Bitset *bitset, size_t shift);
// Convert bitset to string
char *bitset_to_string_normal(const Bitset *bitset);


// Compare two bitsets
int bitset_compare(const Bitset *a, const Bitset *b);

Bitset *bitset_subtract(const Bitset *a, const Bitset *b);

Bitset *int_to_bitset(int number);
Bitset *bcd_correction(Bitset *result);

Bitset *bcd_multiply(const Bitset *a, const Bitset *b);


int main()
{
    Bitset *num1 = NULL;
    Bitset *num2 = NULL;
    int choice;
    int input_num;

    // Initialize BCD mask
    mask_0110 = bitset_create(4);
    bitset_set(mask_0110, 1, true);
    bitset_set(mask_0110, 2, true);

    while (1)
    {
        printf("\nCurrent Numbers:\n");
        printf("Number 1: %s\n", num1 ? bitset_to_string_normal(num1) : "Not set");
        printf("Number 2: %s\n", num2 ? bitset_to_string_normal(num2) : "Not set");
        //     printf("Negative flags: %d, %d\n", num1 ? negative_number : 0, num2 ? negative_number : 0);

        printf("\nMenu:\n");
        printf("1. Enter Number 1\n");
        printf("2. Enter Number 2\n");
        printf("3. Add (Number 1 + Number 2)\n");
        printf("4. Subtract (Number 1 - Number 2)\n");
        printf("5. Multiply (Number 1 * Number 2)\n");
        printf("6. Compare (Number 1 <=> Number 2)\n");
        printf("7. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        switch (choice)
        {
            case 1: // Enter Number 1
                printf("Enter integer for Number 1: ");
                scanf("%d", &input_num);
                fflush(stdin);
                if (num1)
                    bitset_free(num1);
                num1 = int_to_bitset(input_num);
                break;

            case 2: // Enter Number 2
                printf("Enter integer for Number 2: ");
                scanf("%d", &input_num);
                fflush(stdin);
                if (num2)
                    bitset_free(num2);
                num2 = int_to_bitset(input_num);
                break;

            case 3: // Add
                if (num1 && num2)
                {
                    Bitset *sum = bitset_add_with_carry(num1, num2);
                    printf("Sum: %s\n", bitset_to_string_normal(sum));
                    bitset_free(sum);
                }
                else
                {
                    printf("Error: Both numbers must be set first.\n");
                }
                break;

            case 4: // Subtract
                if (num1 && num2)
                {
                    Bitset *diff = bitset_subtract(num1, num2);
                    printf("Difference: %s (%s)\n",
                           bitset_to_string_normal(diff),
                           negative ? "negative" : "positive");
                    bitset_free(diff);
                    negative = false;
                }
                else
                {
                    printf("Error: Both numbers must be set first.\n");
                }
                break;

            case 5: // Multiply
                if (num1 && num2)
                {
                    Bitset *prod = bcd_multiply(num1, num2);
                    printf("Prod: %s\n", bitset_to_string_normal(prod));
                    bitset_free(prod);
                }
                else
                {
                    printf("Error: Both numbers must be set first.\n");
                }
                break;

            case 6: // Compare
                if (num1 && num2)
                {
                    int cmp = bitset_compare(num1, num2);
                    if (cmp < 0)
                    {
                        printf("Number 1 < Number 2\n");
                    }
                    else if (cmp > 0)
                    {
                        printf("Number 1 > Number 2\n");
                    }
                    else
                    {
                        printf("Number 1 == Number 2\n");
                    }
                }
                else
                {
                    printf("Error: Both numbers must be set first.\n");
                }
                break;

            case 7: // Exit
                if (num1)
                    bitset_free(num1);
                if (num2)
                    bitset_free(num2);
                bitset_free(mask_0110);
                return 0;

            default:
                printf("Invalid choice. Please try again.\n");
        }
    }
}

Bitset *bitset_create(size_t size)
{
    Bitset *bitset = (Bitset *)malloc(sizeof(Bitset));
    bitset->size = size;
    bitset->data = (unsigned long *)calloc((size + BITSET_WORD_SIZE - 1) / BITSET_WORD_SIZE, sizeof(unsigned long));
    return bitset;
}

void bitset_free(Bitset *bitset)
{
    if (bitset != NULL)
    {
        free(bitset->data); // Free the data array
        free(bitset);       // Free the bitset structure itself
    }
}

void bitset_set(Bitset *bitset, size_t index, bool value)
{
    if (index >= bitset->size)
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
    if (index >= bitset->size)
        return false;
    size_t word_index = index / BITSET_WORD_SIZE;
    size_t bit_index = index % BITSET_WORD_SIZE;
    return (bitset->data[word_index] & (1UL << bit_index)) != 0;
}


Bitset *bitset_resize(const Bitset *bitset, size_t new_size)
{
    Bitset *resized = bitset_create(new_size);

    // Copy bits from the original bitset to the resized bitset
    size_t min_size = (bitset->size < new_size) ? bitset->size : new_size;
    for (size_t i = 0; i < min_size; i++)
    {
        bool bit = bitset_test(bitset, i);
        bitset_set(resized, i, bit);
    }

    return resized;
}

Bitset *bitset_add_without_carry(const Bitset *a, const Bitset *b)
{
    if (a->size > b->size)
    {
        b = bitset_resize(b, a->size);
    }
    else
    {
        a = bitset_resize(a, b->size);
    }

    Bitset *result = bitset_create(a->size);
    bool carry = false;

    // Iterate from LSB (index 0) to MSB (index size - 1)
    for (size_t i = 0; i < a->size; i++)
    {
        bool bit_a = bitset_test(a, i);
        bool bit_b = bitset_test(b, i);

        // Sum of bits and carry
        bool sum = bit_a ^ bit_b ^ carry;

        // Update carry for the next iteration
        carry = (bit_a && bit_b) || (bit_a && carry) || (bit_b && carry);

        // Set the result bit
        bitset_set(result, i, sum);
    }

    // If there's a carry left after the loop, it indicates overflow
    // if (carry)
    // {
    //     printf("Warning: Overflow detected during addition.\n");
    // }
    // result = bcd_correction(result);
    return result;
}

Bitset *bitset_add_with_carry(const Bitset *a, const Bitset *b)
{
    if (a->size > b->size)
    {
        b = bitset_resize(b, a->size);
    }
    else
    {
        a = bitset_resize(a, b->size);
    }

    Bitset *result = bitset_create(a->size);
    bool carry = false;

    // Iterate through each 4-bit BCD digit (from LSB to MSB)
    for (size_t i = 0; i < a->size; i += 4)
    {
        bool digit_carry = carry;
        carry = false;

        // Process each of the 4 bits in the BCD digit
        for (size_t j = 0; j < 4 && (i + j) < a->size; j++)
        {
            bool bit_a = bitset_test(a, i + j);
            bool bit_b = bitset_test(b, i + j);

            // Sum of bits and carry
            bool sum = bit_a ^ bit_b ^ digit_carry;

            // Update carry for the next bit
            digit_carry = (bit_a && bit_b) || (bit_a && digit_carry) || (bit_b && digit_carry);

            // Set the result bit
            bitset_set(result, i + j, sum);
        }

        // Check if we need BCD correction (sum > 9 or there was a carry)
        bool needs_correction = digit_carry;
        if (!needs_correction)
        {
            // Calculate the decimal value of the 4-bit sum
            int sum_value = 0;
            for (size_t j = 0; j < 4 && (i + j) < a->size; j++)
            {
                if (bitset_test(result, i + j))
                {
                    sum_value += (1 << j);
                }
            }
            needs_correction = (sum_value > 9);
        }

        // Apply BCD correction if needed
        if (needs_correction)
        {
            carry = true; // Carry to next digit

            // Add 6 (0110) to the current digit
            bool correction_carry = false;
            for (size_t j = 0; j < 4 && (i + j) < a->size; j++)
            {
                bool correction_bit = (j == 1 || j == 2); // 0110
                bool current_bit = bitset_test(result, i + j);

                bool new_bit = current_bit ^ correction_bit ^ correction_carry;
                correction_carry = (current_bit && correction_bit) ||
                                   (current_bit && correction_carry) ||
                                   (correction_bit && correction_carry);

                bitset_set(result, i + j, new_bit);
            }
        }
        else
        {
            carry = digit_carry;
        }
    }

    // Handle final carry if needed
    if (carry)
    {
        printf("Warning: Overflow detected during addition. Resizing bitset by 4 bits.\n");
        Bitset *resized_result = bitset_resize(result, result->size + 4);
        bitset_free(result);
        result = resized_result;
        bitset_set(result, a->size, 1); // Set the overflow bit
    }

    return result;
}

void bitset_shift_left(Bitset *bitset, size_t shift)
{
    for (size_t i = bitset->size - 1; i >= shift; i--)
    {
        bitset_set(bitset, i, bitset_test(bitset, i - shift));
    }
    for (size_t i = 0; i < shift; i++)
    {
        bitset_set(bitset, i, false);
    }
}

char *bitset_to_string_normal(const Bitset *bitset)
{
    char *str = (char *)malloc(bitset->size + 1);
    for (size_t i = 0; i < bitset->size; i++)
    {
        str[bitset->size - 1 - i] = bitset_test(bitset, i) ? '1' : '0';
    }
    str[bitset->size] = '\0';
    return str;
}

int bitset_compare(const Bitset *a, const Bitset *b)
{
    // Handle NULL cases
    if (a == NULL && b == NULL)
        return 0;
    if (a == NULL)
        return -1;
    if (b == NULL)
        return 1;

    // Compare sizes first (more bits means larger number)
    if (a->size > b->size)
        return 1;
    if (a->size < b->size)
        return -1;

    // Compare bit by bit from MSB (front) to LSB (back)
    for (size_t i = a->size; i-- > 0;)
    { // Note: counting down from size-1 to 0
        bool bit_a = bitset_test(a, i);
        bool bit_b = bitset_test(b, i);

        if (bit_a && !bit_b)
            return 1; // a has 1 where b has 0
        if (!bit_a && bit_b)
            return -1; // a has 0 where b has 1
    }

    // All bits are equal
    return 0;
}

Bitset *bitset_subtract(const Bitset *a, const Bitset *b)
{
    int res = bitset_compare(a, b);
    if (a->size > b->size)
    {
        b = bitset_resize(b, a->size);
    }
    else if (b->size > a->size)
    {
        a = bitset_resize(a, b->size);
    }

    // Step 1: Compute 9's complement of b
    Bitset *nines = bitset_create(a->size);
    for (size_t i = 0; i < a->size; i += 4)
    {
        unsigned digit = 0;
        for (size_t j = 0; j < 4 && (i + j) < a->size; j++)
        {
            if (bitset_test(b, i + j))
                digit += (1 << j);
        }
        digit = 9 - digit; // 9's complement
        for (size_t j = 0; j < 4 && (i + j) < a->size; j++)
        {
            bitset_set(nines, i + j, (digit >> j) & 1);
        }
    }
    nines = bcd_correction(nines);

    // Step 2: Compute 10's complement (9's + 1)
    Bitset *one = bitset_create(4);
    bitset_set(one, 0, true); // 0001
    Bitset *tens = bitset_add_without_carry(nines, one);
    tens = bcd_correction(tens);

    // Step 3: Add a + 10's complement of b
    Bitset *result = bitset_add_without_carry(a, tens);
    result = bcd_correction(result);

    // Step 4: Check if result is negative (MSD >= 5)
    negative = false;
    if (result->size >= 4)
    {
        unsigned msd = 0;
        for (size_t i = 0; i < 4; i++)
        {
            if (bitset_test(result, result->size - 4 + i))
            {
                msd += (1 << i);
            }
        }
        if (msd >= 5)
        {
            negative = true;

            // Step 5: Compute magnitude (10's complement of result)
            Bitset *magnitude = bitset_create(result->size);
            for (size_t i = 0; i < magnitude->size; i += 4)
            {
                unsigned digit = 0;
                for (size_t j = 0; j < 4 && (i + j) < magnitude->size; j++)
                {
                    if (bitset_test(result, i + j))
                        digit += (1 << j);
                }
                digit = 9 - digit; // 9's complement
                for (size_t j = 0; j < 4 && (i + j) < magnitude->size; j++)
                {
                    bitset_set(magnitude, i + j, (digit >> j) & 1);
                }
            }
            magnitude = bcd_correction(magnitude);

            // Add 1 to get 10's complement
            Bitset *final = bitset_add_without_carry(magnitude, one);
            final = bcd_correction(final);

            bitset_free(result);
            result = final;
        }
    }

    // Cleanup
    bitset_free(nines);
    bitset_free(tens);
    bitset_free(one);
    if (res >= 0)
    {
        result = bitset_resize(result, result->size - 4);
    }
    return result;
}

Bitset *int_to_bitset(int number)
{
    // Handle negative numbers
    negative_number = false;
    if (number < 0)
    {
        negative_number = true;
        number = -number; // Work with absolute value
    }

    // Count number of digits
    int temp = number;
    int digit_count = 0;
    do
    {
        temp /= 10;
        digit_count++;
    } while (temp > 0);

    // Create bitset with 4 bits per digit
    Bitset *bitset = bitset_create(digit_count * 4);

    // Convert each decimal digit to 4-bit BCD
    temp = number;
    for (int i = 0; i < digit_count; i++)
    {
        int digit = temp % 10;
        temp /= 10;

        // Set the 4 bits for this digit
        for (int j = 0; j < 4; j++)
        {
            bool bit = (digit >> j) & 1;
            bitset_set(bitset, i * 4 + j, bit);
        }
    }

    return bitset;
}

Bitset *bcd_correction(Bitset *result)
{
    // Initialize the 0110 mask if needed
    if (mask_0110 == NULL)
    {
        mask_0110 = bitset_create(4);
        bitset_set(mask_0110, 1, true); // 0110 mask (6 in binary)
        bitset_set(mask_0110, 2, true);
    }

    bool carry = false;
    size_t total_nibbles = (result->size + 3) / 4; // Round up

    // Process each nibble from LSB (right) to MSB (left)
    for (size_t nibble = 0; nibble < total_nibbles; nibble++)
    {
        size_t nibble_start = nibble * 4;
        size_t nibble_end = nibble_start + 4;
        if (nibble_end > result->size)
            nibble_end = result->size;

        // Calculate current nibble value
        unsigned value = 0;
        for (size_t i = nibble_start; i < nibble_end; i++)
        {
            if (bitset_test(result, i))
            {
                value += (1 << (i - nibble_start));
            }
        }

        // Apply carry from previous nibble
        if (carry)
        {
            value += 1;
            carry = false;
        }

        // Check if correction needed (value > 9)
        if (value > 9)
        {
            value += 6;
            carry = true;
        }

        // Update the nibble bits
        for (size_t i = nibble_start; i < nibble_end; i++)
        {
            bitset_set(result, i, (value >> (i - nibble_start)) & 1);
        }
    }

    // Handle final carry by adding new nibble at FRONT (MSB side)
    if (carry)
    {
        result = bitset_resize(result, result->size + 4);

        // Add new nibble (0001) at the FRONT (MSB side)
        // Note: Assuming bit 0 is MSB in your representation
        bitset_set(result, result->size - 4, true); // LSB of new nibble (bit 3)
    }

    return result;
}

// Main BCD multiplication function
Bitset *bcd_multiply(const Bitset *a, const Bitset *b)
{
    // Ensure both operands have the same size
    size_t max_size = (a->size > b->size) ? a->size : b->size;
    Bitset *a_resized = bitset_resize(a, max_size);
    Bitset *b_resized = bitset_resize(b, max_size);

    // Initialize result with enough space (2 digits per input digit)
    Bitset *result = bitset_create(max_size);

    // Process each nibble in b (from LSB to MSB)
    for (size_t nibble_pos = 0; nibble_pos < b_resized->size; nibble_pos += 4)
    {
        // Extract current digit from b
        unsigned digit = 0;
        for (size_t j = 0; j < 4 && (nibble_pos + j) < b_resized->size; j++)
        {
            if (bitset_test(b_resized, nibble_pos + j))
            {
                digit += (1 << j);
            }
        }

        if (digit == 0)
            continue;

        // Multiply a by this digit
        Bitset *partial = bitset_create(max_size * 2);
        Bitset *temp = bitset_create(max_size);
        for (size_t i = 0; i < a_resized->size; i++)
        {
            bitset_set(temp, i, bitset_test(a_resized, i));
        }

        for (size_t bit_pos = 0; bit_pos < 4; bit_pos++)
        {
            if ((digit >> bit_pos) & 1)
            {
                Bitset *shifted = bitset_create(temp->size + bit_pos);
                for (size_t i = 0; i < temp->size; i++)
                {
                    bitset_set(shifted, i + bit_pos, bitset_test(temp, i));
                }
                shifted = bcd_correction(shifted);

                Bitset *new_partial = bitset_add_with_carry(partial, shifted);
                bitset_free(partial);
                bitset_free(shifted);
                partial = new_partial;
                partial = bcd_correction(partial);
            }
        }
        bitset_free(temp);

        // Shift according to nibble position (×10^n)
        for (size_t shift = 0; shift < (nibble_pos / 4); shift++)
        {
            // Shift left by 4 bits (×10)
            for (int i = 0; i < 4; i++)
            {
                bitset_shift_left(partial, 1);
                partial = bcd_correction(partial);
            }
        }

        // Add to result
        Bitset *new_result = bitset_add_with_carry(result, partial);
        bitset_free(result);
        bitset_free(partial);
        result = new_result;
        result = bcd_correction(result);
    }

    // Clean up
    bitset_free(a_resized);
    bitset_free(b_resized);

    return result;
}
