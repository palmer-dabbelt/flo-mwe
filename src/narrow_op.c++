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

#include "narrow_op.h++"

typedef std::vector<std::shared_ptr<libflo::operation<narrow_node>>> out_t;

out_t narrow_op(const std::shared_ptr<libflo::operation<wide_node>> op,
                size_t width)
{
    /* Check to see if this operation just fits within a machine word,
     * in which case we don't really have to do anything. */
    {
        bool too_large = false;
        for (auto it = op->operands(); !it.done(); ++it) {
            auto op = *it;
            if (op->width() > width)
                too_large = true;
        }

        /* If none of the operands were too large to fit within a
         * machine word then just do the cast. */
        if (too_large == false) {
            std::shared_ptr<narrow_node> d;
            std::vector<std::shared_ptr<narrow_node>> s;

            d = narrow_node::clone_from(op->d());
            for (auto it = op->sources(); !it.done(); ++it)
                s.push_back(narrow_node::clone_from(*it));

            auto ptr = libflo::operation<narrow_node>::create(d,
                                                              op->width_u(),
                                                              op->op(),
                                                              s);

            out_t out;
            out.push_back(ptr);
            return out;
        }
    }

    /* At this point we know the operation is too wide so we need to
     * remap it to some narrower operations. */
    out_t out;

    /* FIXME: Actually do this... */
    fprintf(stderr, "Unimplemented narrowing\n");
    abort();

    return out;
}
