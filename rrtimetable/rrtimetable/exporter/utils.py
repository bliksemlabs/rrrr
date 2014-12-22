import sys, struct, time
from struct import Struct

############################

#pack arrival and departure into 4 bytes w/ offset?

# C structs, must match those defined in .h files.

struct_1I = Struct('I') # a single UNSIGNED int
def writeint(out,x) :
    out.write(struct_1I.pack(x));

struct_1H = Struct('H') # a single UNSIGNED short
def writeshort(out,x) :
    out.write(struct_1H.pack(x));

struct_1B = Struct('B') # a single UNSIGNED byte
def writebyte(out,x) :
    out.write(struct_1B.pack(x));

struct_2H = Struct('HH') # a two UNSIGNED shorts
def write_2ushort(out,x, y) :
    out.write(struct_2H.pack(x, y));

struct_2f = Struct('2f') # 2 floats
def write2floats(out,x, y) :
    out.write(struct_2f.pack(x, y));

def align(out,width=4) :
    """ Align output file to a [width]-byte boundary. """
    pos = out.tell()
    n_padding_bytes = (width - (pos % width)) % width
    out.write('%' * n_padding_bytes)

def tell(out) :
    """ Display the current output file position in a human-readable format, then return that position in bytes. """
    pos = out.tell()
    if pos > 1024 * 1024 :
        text = '%0.2f MB' % (pos / 1024.0 / 1024.0)
    else :
        text = '%0.2f kB' % (pos / 1024.0)
    print "  at position %d in output [%s]" % (pos, text)
    return pos

def write_text_comment(out,string) :
    """ Write a text block to the file, just to help indentify segment boundaries, and align. """
    string = '|| {:s} ||'.format(string)
    out.write(string)
    align(out)

def write_string_table(out,strings) :
    """ Write a table of fixed-width, null-terminated strings to the output file.
    The argument is a list of Python strings.
    The output string table begins with an integer indicating the width of each entry in bytes (including the null terminator).
    This is followed by N*W bytes of string data.
    This data structure can provide a mapping from integer IDs to strings and vice versa:
    If the strings are sorted, a binary search can be used for string --> ID lookups in logarithmic time.
    Note: Later we could use fixed width non-null-terminated string: printf("%.*s", length, string); or fwrite();
    """
    # sort a copy of the string list
    # strings = list(strings)
    # strings.sort()
    width = 0;
    for s in strings :
        if len(s) > width :
            width = len(s)
    width += 1
    loc = tell(out)
    writeint(out,width)
    for s in strings :
        out.write(s)
        padding = '\0' * (width - len(s))
        out.write(padding)
    return loc

