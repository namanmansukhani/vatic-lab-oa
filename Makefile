CXX ?= g++
CXXFLAGS ?= -std=c++14 -O2 -Wall -Wextra -pedantic
SHELL := /bin/bash

TARGET := solution
SOURCE := solution.cpp

.PHONY: all clean check

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)

check: $(TARGET)
	@set -e; \
	for test_case in tests/*; do \
		input="$$test_case/input.txt"; \
		output="$$test_case/output.txt"; \
		echo "Checking $${input}"; \
		diff <(./$(TARGET) < "$$input") "$$output" || { \
			echo "Test failed: $${input}"; \
			exit 1; \
		}; \
		echo "Success: $${input}"; \
	done

submit: clean all
	zip -r submission.zip $(SOURCE) $(TARGET) tests Makefile README.md