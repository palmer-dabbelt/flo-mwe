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

narrow_node::narrow_node(const std::string name,
                         const libflo::unknown<size_t>& width,
                         const libflo::unknown<size_t>& depth,
                         bool is_mem,
                         bool is_const,
                         libflo::unknown<size_t> cycle)
    : libflo::node(name, width, depth, is_mem, is_const, cycle)
{
    if (this->width() > wide_node::get_word_length()) {
        fprintf(stderr, "Attempted to build a narrow node wider than a word\n");
        abort();
    }
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
