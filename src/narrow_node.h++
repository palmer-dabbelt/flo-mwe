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

#ifndef NARROW_NODE_HXX
#define NARROW_NODE_HXX

#include <libflo/node.h++>

/* Holds a narrow node, which is a node that will always fit within a
 * single word on the target machine. */
class narrow_node: public libflo::node {
    friend class libflo::node;

public:
    narrow_node(const std::string name,
                const libflo::unknown<size_t>& width,
                const libflo::unknown<size_t>& depth,
                bool is_mem,
                bool is_const,
                libflo::unknown<size_t> cycle);
};

#endif
