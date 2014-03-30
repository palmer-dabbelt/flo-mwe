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
#include <libflo/sizet_printf.h++>

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

static std::vector<std::shared_ptr<shallow_node>>
map_shallow(const std::string name,
            const libflo::unknown<size_t>& width,
            const libflo::unknown<size_t>& depth,
            bool is_mem,
            bool is_const,
            libflo::unknown<size_t> cycle);

narrow_node::narrow_node(const std::string name,
                         const libflo::unknown<size_t>& width,
                         const libflo::unknown<size_t>& depth,
                         bool is_mem,
                         bool is_const,
                         libflo::unknown<size_t> cycle)
    : libflo::node(name, width, depth, is_mem, is_const, cycle),
      _sns(),
      _sns_valid(false)
{
    if (this->width() > wide_node::get_word_length()) {
        fprintf(stderr, "Attempted to build a narrow node wider than a word\n");
        abort();
    }
}

narrow_node::narrow_node(const std::string name,
                         const libflo::unknown<size_t>& width,
                         const libflo::unknown<size_t>& depth,
                         bool is_mem,
                         bool is_const,
                         libflo::unknown<size_t> cycle,
                         bool is_catd)
    : libflo::node(name, width, depth, is_mem, is_const, cycle),
      _sns(),
      _sns_valid(false)
{
    if (!is_catd && (this->width() > wide_node::get_word_length())) {
        fprintf(stderr, "Attempted to build a narrow node wider than a word\n");
        abort();
    }
}

narrow_node::snode_viter narrow_node::snodes(void)
{
    if (_sns_valid == false) {
        auto to_add = map_shallow(name(),
                                  width_u(),
                                  depth_u(),
                                  is_mem(),
                                  is_const(),
                                  cycle_u()
            );

        for (auto it = to_add.begin(); it != to_add.end(); ++it)
            _sns.push_back(*it);

        _sns_valid = true;
    }

    return snode_viter(_sns);
}

std::shared_ptr<shallow_node> narrow_node::snode(size_t i)
{
    if (_sns_valid == false) {
        auto to_add = map_shallow(name(),
                                  width_u(),
                                  depth_u(),
                                  is_mem(),
                                  is_const(),
                                  cycle_u()
            );

        for (auto it = to_add.begin(); it != to_add.end(); ++it)
            _sns.push_back(*it);

        _sns_valid = true;
    }

    return _sns[i];
}

std::shared_ptr<narrow_node>
narrow_node::clone_from(std::shared_ptr<wide_node> w)
{
    return std::shared_ptr<narrow_node>(new narrow_node(w->name(),
                                                        w->width_u(),
                                                        w->depth_u(),
                                                        w->is_mem(),
                                                        w->is_const(),
                                                        w->cycle_u()));
}

std::shared_ptr<narrow_node>
narrow_node::clone_from(std::shared_ptr<wide_node> w, bool force)
{
    return std::shared_ptr<narrow_node>(new narrow_node(w->name(),
                                                        w->width_u(),
                                                        w->depth_u(),
                                                        w->is_mem(),
                                                        w->is_const(),
                                                        w->cycle_u(),
                                                        force));
}

std::shared_ptr<narrow_node>
narrow_node::create_temp(const std::shared_ptr<narrow_node> t)
{
    static unsigned long num = 0;

    char name[LINE_MAX];
    snprintf(name, LINE_MAX, "MWEnT%lu", num++);

    return std::shared_ptr<narrow_node>(new narrow_node(name,
                                                        t->width_u(),
                                                        t->depth_u(),
                                                        t->is_mem(),
                                                        t->is_const(),
                                                        t->cycle_u()
                                            ));
}

std::shared_ptr<narrow_node>
narrow_node::create_temp(const size_t width)
{
    static unsigned long num = 0;

    char name[LINE_MAX];
    snprintf(name, LINE_MAX, "MWEnW%lu", num++);

    return std::shared_ptr<narrow_node>(
        new narrow_node(name,
                        width,
                        0,
                        false,
                        false,
                        libflo::unknown<size_t>()
            ));
}

std::shared_ptr<narrow_node>
narrow_node::create_const(const std::shared_ptr<narrow_node> t, size_t value)
{
    char name[LINE_MAX];
    snprintf(name, LINE_MAX, SIZET_FORMAT, value);

    return std::shared_ptr<narrow_node>(new narrow_node(name,
                                                        t->width_u(),
                                                        t->depth_u(),
                                                        t->is_mem(),
                                                        t->is_const(),
                                                        t->cycle_u()
                                            ));
}

std::vector<std::shared_ptr<shallow_node>>
map_shallow(const std::string name,
            const libflo::unknown<size_t>& width,
            const libflo::unknown<size_t>& depth,
            bool is_mem,
            bool is_const,
            libflo::unknown<size_t> cycle)
{
    std::vector<std::shared_ptr<shallow_node>> out;

    /* Here's the number of nodes we need to build from this node. */
    const size_t node_count = (depth.value() == 0) ? 1 :
        (depth.value() + wide_node::get_mem_depth() - 1)
        / wide_node::get_mem_depth();

    for (size_t i = 0; i < node_count; ++i) {
        char n[LINE_MAX];

        /* Avoids mangling node names that don't actually need to be
         * mangled! */
        if (node_count == 1) {
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
        libflo::unknown<size_t> d = depth;
        if (i != (node_count - 1))
            d = wide_node::get_mem_depth();
        else if (i > 0)
            d = ((depth.value() - 1) % wide_node::get_mem_depth()) + 1;

        auto ptr = new shallow_node(n, width, d, is_mem, is_const, cycle);
        out.push_back(std::shared_ptr<shallow_node>(ptr));
    }

    return out;
}
