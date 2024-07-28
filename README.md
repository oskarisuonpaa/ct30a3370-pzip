# ct30a3370-pzip
Simple parallized version of UNIX ZIP.

## Compiling
```
gcc -o pzip pzip.c -pthread -Wall -Werror
```

## Usage

```
./pzip file1 [file2 ...] > output_file.z
```
