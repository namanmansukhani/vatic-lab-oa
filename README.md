## Order Matching Engine

Project Tree:
```
.
├── Makefile
├── README.md
├── solution
├── solution.cpp
└── tests
    └── test1
        ├── input.txt
        └── output.txt
    └── ....
    └── testN
        ├── input.txt
        └── output.txt
```

### Instructions

1. Compile code:
```
make
```

This creates an executable called `solution` in the toplevel directory.

2. Run test cases:

```
make check
```

#####  Note: test cases assume that the output file ends in a newline, because `make check` uses `diff`.

3. Clean
```
make clean
```