# Original sequence
sequence = [0, 1, 4, 5, 1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11, 1, 0, 5, 4, 0, 9, 4, 9, 2, 11, 6, 11, 3, 10, 7, 10, 2, 3, 6, 7, 11, 2, 11, 6, 9, 0, 9, 4, 8, 1, 8, 5, 3, 2, 7, 6, 10, 3, 10, 7, 8, 1, 8, 5, 9, 0, 9, 4, 6, 7, 2, 3, 11, 6, 11, 2, 5, 4, 1, 0, 8, 5, 8, 1, 7, 6, 3, 2, 10, 7, 10, 3, 8, 5, 8, 1, 9, 4, 9, 0, 4, 5, 0, 1, 5, 8, 1, 8, 7, 10, 3, 10, 6, 11, 2, 11, 5, 4, 1, 0, 4, 9, 0, 9, 6, 11, 2, 11, 7, 10, 3, 10, 6, 11, 2, 11, 7, 6, 3, 2, 5, 8, 1, 8, 4, 5, 0, 1, 7, 10, 3, 10, 6, 7, 2, 3, 4, 9, 0, 9, 5, 8, 1, 8, 8, 5, 8, 1, 5, 4, 1, 0, 11, 6, 11, 2, 10, 7, 10, 3, 9, 4, 9, 0, 4, 5, 0, 1, 10, 7, 10, 3, 11, 6, 11, 2, 8, 1, 8, 5, 1, 0, 5, 4, 11, 2, 11, 6, 2, 3, 6, 7, 9, 0, 9, 4, 0, 1, 4, 5, 10, 3, 10, 7, 11, 2, 11, 6, 2, 11, 6, 11, 3, 2, 7, 6, 1, 8, 5, 8, 0, 9, 4, 9, 3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 8, 5, 8, 3, 10, 7, 10, 2, 3, 6, 7, 0, 9, 4, 9, 1, 0, 5, 4, 2, 11, 6, 11, 3, 10, 7, 10, 1, 8, 5, 8, 0, 1, 4, 5, 9, 0, 9, 4, 8, 1, 8, 5, 10, 3, 10, 7, 3, 2, 7, 6, 8, 1, 8, 5, 9, 0, 9, 4, 11, 2, 11, 6, 2, 3, 6, 7, 9, 4, 9, 0, 4, 5, 0, 1, 10, 7, 10, 3, 7, 6, 3, 2, 8, 5, 8, 1, 9, 4, 9, 0, 11, 6, 11, 2, 6, 7, 2, 3, 7, 10, 3, 10, 6, 11, 2, 11, 4, 9, 0, 9, 5, 4, 1, 0, 6, 11, 2, 11, 7, 10, 3, 10, 5, 8, 1, 8, 4, 5, 0, 1, 5, 4, 1, 0, 4, 9, 0, 9, 6, 7, 2, 3, 7, 10, 3, 10, 4, 9, 0, 9, 5, 8, 1, 8, 7, 6, 3, 2, 6, 11, 2, 11, 11, 6, 11, 2, 10, 7, 10, 3, 4, 5, 0, 1, 9, 4, 9, 0, 10, 7, 10, 3, 11, 6, 11, 2, 5, 4, 1, 0, 8, 5, 8, 1, 3, 2, 7, 6, 10, 3, 10, 7, 0, 1, 4, 5, 9, 0, 9, 4, 10, 3, 10, 7, 11, 2, 11, 6, 1, 0, 5, 4, 8, 1, 8, 5, 1, 8, 5, 8, 0, 9, 4, 9, 2, 3, 6, 7, 3, 10, 7, 10, 0, 9, 4, 9, 1, 8, 5, 8, 3, 2, 7, 6, 2, 11, 6, 11]

# Find the maximum number in the sequence to determine the bit length
max_num = max(sequence)
max_bits = len(format(max_num, 'b'))

# Convert each number to a binary string and pad with leading zeros to match the maximum bit length
binary_sequence = [format(num, f'0{max_bits}b') for num in sequence]
binary_sequence = list(reversed(binary_sequence))

# Initialize empty strings for each bit position sequence
bit_sequences = ['' for _ in range(max_bits)]

# Split the binary strings based on their bit positions and concatenate to the respective bit sequence strings
for binary in binary_sequence:
    for i in range(max_bits):
        bit_sequences[i] += binary[i]
bit_sequences = list(reversed(bit_sequences))

# Convert binary sequences to hexadecimal
hex_sequences = [format(int(bit_seq, 2), 'x') for bit_seq in bit_sequences]

# Print the results
print("Original sequence:", sequence)
print("Binary sequence:", binary_sequence)
for i, (bit_seq, hex_seq) in enumerate(zip(bit_sequences, hex_sequences)):
    print(f"Bit {i} sequence (binary):", bit_seq)

for i, (bit_seq, hex_seq) in enumerate(zip(bit_sequences, hex_sequences)):
    print(f"Bit {i} sequence (hex):", hex_seq)