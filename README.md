# Simple Naive Bayes Spam Classifier

A basic implementation of a Naive Bayes classifier for spam detection in C.

## Usage
```bash
gcc -o spam main.c -lm
./spam "Your message here"
```

## Example
```bash
./spam "CONGRATULATIONS! You've won a FREE iPhone! Click here NOW!!!"
# Output: SPAM 

./spam "Wanna grab coffee later?"
# Output: HAM 
```

## Requirements

- `uthash.h` (included)
- `spam.csv` training dataset in the same directory
