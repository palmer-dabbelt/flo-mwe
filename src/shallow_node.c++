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

#include "shallow_node.h++"

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

shallow_node::shallow_node(const std::string name,
                         const libflo::unknown<size_t>& width,
                         const libflo::unknown<size_t>& depth,
                         bool is_mem,
                         bool is_const,
                         libflo::unknown<size_t> cycle)
    : libflo::node(name, width, depth, is_mem, is_const, cycle)
{
    if (this->depth() > wide_node::get_mem_depth()) {
        fprintf(stderr, "Attempted to build a shallow that's too deep\n");
        abort();
    }
}

std::shared_ptr<shallow_node>
shallow_node::clone_from(std::shared_ptr<narrow_node> w)
{
    return std::shared_ptr<shallow_node>(new shallow_node(w->name(),
                                                          w->width_u(),
                                                          w->depth_u(),
                                                          w->is_mem(),
                                                          w->is_const(),
                                                          w->cycle_u()));
}


std::shared_ptr<shallow_node>
shallow_node::create_temp(const std::shared_ptr<shallow_node> t)
{

    static unsigned long num = 0;

    char name[LINE_MAX];
    snprintf(name, LINE_MAX, "MWEsT%lu", num++);

    return std::shared_ptr<shallow_node>(new shallow_node(name,
                                                          t->width_u(),
                                                          t->depth_u(),
                                                          t->is_mem(),
                                                          t->is_const(),
                                                          t->cycle_u()
                                             ));

}

std::shared_ptr<shallow_node>
shallow_node::create_temp(const size_t width)
{
    static unsigned long num = 0;

    char name[LINE_MAX];
    snprintf(name, LINE_MAX, "MWEsW%lu", num++);

    return std::shared_ptr<shallow_node>(
        new shallow_node(name,
                        width,
                        0,
                        false,
                        false,
                        libflo::unknown<size_t>()
            ));
}

std::shared_ptr<shallow_node>
shallow_node::create_const(const std::shared_ptr<shallow_node> t, size_t value)
{
    char name[LINE_MAX];
    snprintf(name, LINE_MAX, "%lu", value);

    return std::shared_ptr<shallow_node>(new shallow_node(name,
                                                          t->width_u(),
                                                          t->depth_u(),
                                                          t->is_mem(),
                                                          t->is_const(),
                                                          t->cycle_u()
                                             ));
}

std::shared_ptr<shallow_node>
shallow_node::create_const(size_t width, size_t value)
{
    return create_const(create_temp(width), value);
}
