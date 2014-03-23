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

#include "split_mem.h++"
#include <cmath>

typedef std::vector<std::shared_ptr<libflo::operation<shallow_node>>> out_t;

out_t split_mem(const std::shared_ptr<libflo::operation<narrow_node>> op,
                size_t depth)
{
    /* Most node won't be too deep.  In order to avoid screwing
     * anything up I just output those nodes directly. */
    {
        bool too_deep = false;
        for (auto it = op->operands(); !it.done(); ++it) {
            auto op = *it;
            if (op->depth() > depth)
                too_deep = true;
        }

        if (too_deep == false) {
            std::shared_ptr<shallow_node> d;
            std::vector<std::shared_ptr<shallow_node>> s;

            d = shallow_node::clone_from(op->d());
            for (auto it = op->sources(); !it.done(); ++it)
                s.push_back(shallow_node::clone_from(*it));

            out_t out;
            auto ptr = libflo::operation<shallow_node>::create(d,
                                                               op->width_u(),
                                                               op->op(),
                                                               s);
            out.push_back(ptr);
            return out;
        }
    }

    fprintf(stderr, "Unable to shallow-ize node\n");
    abort();
    out_t out;
    return out;
}
