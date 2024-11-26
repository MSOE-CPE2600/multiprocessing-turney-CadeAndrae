CC=gcc
CFLAGS=-c -Wall -g
LDFLAGS=-ljpeg -lm -lpthread -lrt
SOURCES=mandel.c jpegrw.c
MOVIE_SOURCES=mandelmovie.c jpegrw.c
OBJECTS=$(SOURCES:.c=.o)
MOVIE_OBJECTS=$(MOVIE_SOURCES:.c=.o)
EXECUTABLE=mandel
MOVIE_EXECUTABLE=mandelmovie

# Default target: build both executables
all: $(EXECUTABLE) $(MOVIE_EXECUTABLE)

# Compile the mandel executable
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Compile the mandelmovie executable
$(MOVIE_EXECUTABLE): $(MOVIE_OBJECTS)
	$(CC) $(MOVIE_OBJECTS) $(LDFLAGS) -o $@

# Rule for .c to .o compilation
%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
	$(CC) -MM $< > $*.d

# Include dependencies for existing .o files
-include $(OBJECTS:.o=.d) $(MOVIE_OBJECTS:.o=.d)

# Clean up generated files
clean:
	rm -rf $(OBJECTS) $(MOVIE_OBJECTS) $(EXECUTABLE) $(MOVIE_EXECUTABLE) *.d

# Phony targets
.PHONY: all clean
