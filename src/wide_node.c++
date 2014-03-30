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
#include <libflo/sizet_printf.h++>

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

size_t wide_node::_word_length;
bool wide_node::_word_length_set = false;

size_t wide_node::_mem_depth;
bool wide_node::_mem_depth_set = false;

/* Maps this node to a list of narrow nodes. */
static std::vector<std::shared_ptr<narrow_node>>
map_narrow(const std::string name,
           const libflo::unknown<size_t>& width,
           const libflo::unknown<size_t>& depth,
           bool is_mem,
           bool is_const,
           libflo::unknown<size_t> cycle);

static std::vector<std::shared_ptr<narrow_node>>
map_catd(const std::string name,
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
      _nns_valid(false),
      _cdn(),
      _cdn_valid(false)
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

std::shared_ptr<narrow_node> wide_node::nnode(size_t i)
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

    return _nns[i];
}

std::shared_ptr<narrow_node> wide_node::catdnode(size_t i)
{
    if (_cdn_valid == false) {
        auto to_add = map_catd(name(),
                               width_u(),
                               depth_u(),
                               is_mem(),
                               is_const(),
                               cycle_u()
            );

        for (auto it = to_add.begin(); it != to_add.end(); ++it)
            _cdn.push_back(*it);

        _cdn_valid = true;
    }

    return _cdn[i];
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

void wide_node::set_mem_depth(size_t word_length)
{
    if (wide_node::_mem_depth_set == true) {
        fprintf(stderr, "Memory depth set twice!\n");
        abort();
    }

    _mem_depth_set = true;
    _mem_depth = word_length;
}

size_t wide_node::get_mem_depth(void)
{
    if (wide_node::_mem_depth_set == false) {
        fprintf(stderr, "Memory depth get before set\n");
        abort();
    }

    return _mem_depth;
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
            snprintf(n, LINE_MAX, "%s." SIZET_FORMAT, name.c_str(), i);
        }

        /* FIXME: This silently drops the high-order bits of
         * constants... */
        /* Here's a special case for constants -- we only support
         * emiting one-word wide constants and fail on everything
         * else! */
        if (is_const && i == 0)
            snprintf(n, LINE_MAX, "%s", name.c_str());
        else if (is_const)
            snprintf(n, LINE_MAX, "0");

        /* Figure out how wide the output should be.  The general rule
         * is to not touch already-narrow ops, and to map other ops to
         * many nodes that are as wide as possible. */
        libflo::unknown<size_t> w = width;
        if (i != (node_count - 1))
            w = wide_node::get_word_length();
        else if (i > 0)
            w = ((width.value() - 1) % wide_node::get_word_length()) + 1;

        auto ptr = new narrow_node(n, w, depth, is_mem, is_const, cycle);
        out.push_back(std::shared_ptr<narrow_node>(ptr));
    }

    return out;
}

std::vector<std::shared_ptr<narrow_node>>
map_catd(const std::string name,
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
        if (node_count == 1 || i == (node_count - 1)) {
            snprintf(n, LINE_MAX, "%s", name.c_str());
        } else {
            snprintf(n, LINE_MAX, "%s.c" SIZET_FORMAT, name.c_str(), i);
        }

        /* FIXME: This silently drops the high-order bits of
         * constants... */
        /* Here's a special case for constants -- we only support
         * emiting one-word wide constants and fail on everything
         * else! */
        if (is_const && i == 0)
            snprintf(n, LINE_MAX, "%s", name.c_str());
        else if (is_const)
            snprintf(n, LINE_MAX, "0");

        /* Figure out how wide the output should be.  The general rule
         * is to not touch already-narrow ops, and to map other ops to
         * many nodes that are as wide as possible. */
        libflo::unknown<size_t> w = width;
        if (i != (node_count - 1))
            w = (i + 1) * wide_node::get_word_length();
        else if (i > 0)
            w = width.value();

        auto ptr = new narrow_node(n, w, depth, is_mem, is_const, cycle, true);
        out.push_back(std::shared_ptr<narrow_node>(ptr));
    }

    return out;
}
