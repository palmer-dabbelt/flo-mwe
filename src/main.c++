/*
 * Copyright (C) 2014 Palmer Dabbelt
 *   <palmer.dabbelt@eecs.berkeley.edu>
 *
 * This file is part of flo-mwe.
 *
 * flo-mwe is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * flo-mwe is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with flo-mwe.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "narrow_node.h++"
#include "version.h"
#include "wide_node.h++"
#include <libflo/flo.h++>
#include <stdlib.h>
#include <string>
using namespace libflo;

int main(int argc, const char **argv)
{
    /* Prints the version if it was asked for. */
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        fprintf(stderr, "%s\n", PCONFIGURE_VERSION);
        exit(0);
    }

    /* Prints the help text if anything went wrong. */
    if (argc != 7) {
        fprintf(stderr, "%s: --width <w> --input <i> --output <o>\n", argv[0]);
        fprintf(stderr, "  Replaces multi-word operations with multiple\n");
        fprintf(stderr, "  single-word operations in a Flo file.\n");
        fprintf(stderr, "  \n");
        fprintf(stderr, "  --input sets the input filename\n");
        fprintf(stderr, "  --output sets the output length\n");
        exit(1);
    }

    /* Checks for the exact command-line arguments. */
    if (strcmp(argv[1], "--width") != 0) {
        fprintf(stderr, "Expected --width as 1st argument\n");
        exit(1);
    }
    if (strcmp(argv[3], "--input") != 0) {
        fprintf(stderr, "Expected --input as 3rd argument\n");
        exit(1);
    }
    if (strcmp(argv[5], "--output") != 0) {
        fprintf(stderr, "Expected --output as 5th argument\n");
        exit(1);
    }

    /* Finishes parsing the command-line arguments. */
    const size_t arg_width = atoi(argv[2]);
    const std::string arg_input = argv[4];
    const std::string arg_output = argv[6];

    /* This is a bit of a hack: essentially wide nodes have a fixed
     * width parameter that determines the word length of the target
     * machine.  This allows me to generate the set of narrow words
     * that coorespond to this wide word when the wide word is
     * constructed, which is nice because I then don't do any more
     * string munging and just use pointers. */
    wide_node::set_word_length(arg_width);

    /* Here we create two Flo files: one for input an one for output.
     * The input file is read to produce the input one (which is
     * const), and the other one accumulates outputs. */
    auto in_flo = flo<wide_node, operation<wide_node>>::parse(arg_input);
    auto out_flo = flo<narrow_node, operation<narrow_node>>::empty();

    /* Writes the whole output graph that was produced into a Flo
     * file. */
    FILE *out_file = fopen(arg_output.c_str(), "w");
    if (out_file == NULL) {
        perror("fopen() failed");
        fprintf(stderr, "Unable to open '%s' for writing\n",
                arg_output.c_str());
        abort();
    }

    for (auto it = out_flo->operations(); !it.done(); ++it) {
        auto op = *it;
        op->writeln(out_file);
    }

    fclose(out_file);
}
