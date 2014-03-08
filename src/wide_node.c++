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

#include "wide_node.h++"

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

size_t wide_node::_word_length;
bool wide_node::_word_length_set = false;

/* Maps this node to a list of narrow nodes. */
static std::vector<std::shared_ptr<narrow_node>>
map_narrow(const std::string name,
           const libflo::unknown<size_t>& width,
           const libflo::unknown<size_t>& depth,
           bool is_mem,
           bool is_const,
           libflo::unknown<size_t> cycle);

wide_node::wide_node(const std::string name,
                     const libflo::unknown<size_t>& width,
                     const libflo::unknown<size_t>& depth,
                     bool is_mem,
                     bool is_const,
                     libflo::unknown<size_t> cycle)
    : libflo::node(name, width, depth, is_mem, is_const, cycle),
      _nns(),
      _nns_valid(false)
{
}

wide_node::nnode_viter wide_node::nnodes(void)
{
    if (_nns_valid == false) {
        auto to_add = map_narrow(name(),
                                 width_u(),
                                 depth_u(),
                                 is_mem(),
                                 is_const(),
                                 cycle_u()
            );

        for (auto it = to_add.begin(); it != to_add.end(); ++it)
            _nns.push_back(*it);

        _nns_valid = true;
    }

    return nnode_viter(_nns);
}

void wide_node::set_word_length(size_t word_length)
{
    if (wide_node::_word_length_set == true) {
        fprintf(stderr, "Word length set twice!\n");
        abort();
    }

    _word_length_set = true;
    _word_length = word_length;
}

size_t wide_node::get_word_length(void)
{
    if (wide_node::_word_length_set == false) {
        fprintf(stderr, "Word length get before set\n");
        abort();
    }

    return _word_length;
}

std::vector<std::shared_ptr<narrow_node>>
map_narrow(const std::string name,
           const libflo::unknown<size_t>& width,
           const libflo::unknown<size_t>& depth,
           bool is_mem,
           bool is_const,
           libflo::unknown<size_t> cycle)
{
    std::vector<std::shared_ptr<narrow_node>> out;

    /* Here's the number of nodes we need to build from this node. */
    const size_t node_count =
        (width.value() + wide_node::get_word_length() - 1)
        / wide_node::get_word_length();

    for (size_t i = 0; i < node_count; ++i) {
        char n[LINE_MAX];

        /* Avoids mangling node names that don't actually need to be
         * mangled! */
        if (node_count == 1) {
            snprintf(n, LINE_MAX, "%s", name.c_str());
        } else {
            snprintf(n, LINE_MAX, "%s.%lu", name.c_str(), i);
        }

        auto ptr = new narrow_node(n, width, depth, is_mem, is_const, cycle);
        out.push_back(std::shared_ptr<narrow_node>(ptr));
    }

    return out;
}
